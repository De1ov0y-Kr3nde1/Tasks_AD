package main

import (
	"context"
	"fmt"
	"io"
	"log"
	"net/http"
	"os"
	"os/exec"
	"path/filepath"
	"regexp"
	"strings"
	"time"
)

const (
	FakePycPath = "/home/strike17/task_AD/Compile/deploy/fakepyc"
	TempDir     = "/tmp/fakepyc_work"
	ViewHTML    = "/home/strike17/task_AD/Compile/deploy/templates/view.html"

	MaxCodeSize    = 100 * 1024
	RequestTimeout = 30 * time.Second
)

var safeFileNameRe = regexp.MustCompile(`^code_\d+\.py$`)

func init() {
	if err := os.MkdirAll(TempDir, 0700); err != nil {
		log.Fatalf("Не удалось создать TempDir: %v", err)
	}
}

func compileHandler(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		http.Error(w, "Только POST", http.StatusMethodNotAllowed)
		return
	}

	r.Body = http.MaxBytesReader(w, r.Body, MaxCodeSize)
	body, err := io.ReadAll(r.Body)
	if err != nil {
		http.Error(w, "Слишком большой код или ошибка чтения", http.StatusBadRequest)
		return
	}
	defer r.Body.Close()

	code := string(body)
	if strings.TrimSpace(code) == "" {
		http.Error(w, "Код не может быть пустым", http.StatusBadRequest)
		return
	}

	ctx, cancel := context.WithTimeout(r.Context(), RequestTimeout)
	defer cancel()

	ts := time.Now().UnixNano()
	inputFile := filepath.Join(TempDir, fmt.Sprintf("code_%d.py", ts))
	outputFile := filepath.Join(TempDir, fmt.Sprintf("program_%d", ts))

	if !strings.HasPrefix(inputFile, TempDir+"/") || !strings.HasPrefix(outputFile, TempDir+"/") {
		http.Error(w, "Недопустимое имя файла", http.StatusInternalServerError)
		return
	}
	if !safeFileNameRe.MatchString(filepath.Base(inputFile)) {
		http.Error(w, "Недопустимый формат файла", http.StatusInternalServerError)
		return
	}

	if err := os.WriteFile(inputFile, []byte(code), 0600); err != nil {
		log.Printf("[SEC] Ошибка записи входного файла: %v", err)
		http.Error(w, "Ошибка сервера", http.StatusInternalServerError)
		return
	}
	defer func() {
		_ = os.Remove(inputFile)
	}()

	cmd := exec.CommandContext(ctx, FakePycPath, inputFile, outputFile)
	cmd.Dir = TempDir
	cmd.Env = []string{"PATH=/usr/bin:/bin"}

	err = cmd.Run()

	defer func() {
		_ = os.Remove(outputFile)
	}()

	if ctx.Err() == context.DeadlineExceeded {
		log.Println("[SEC] Компиляция превысила лимит времени")
		http.Error(w, "Превышено время компиляции", http.StatusRequestTimeout)
		return
	}

	if err != nil {
		log.Printf("[SEC] fakepyc ошибка: %v", err)
		http.Error(w, "Нет прав", http.StatusBadRequest)
		return
	}

	if _, err := os.Stat(outputFile); os.IsNotExist(err) {
		log.Println("[SEC] Выходной файл не создан после успешного завершения")
		http.Error(w, "Компилятор не создал выходной файл", http.StatusInternalServerError)
		return
	}

	fileInfo, err := os.Stat(outputFile)
	if err != nil || fileInfo.Size() > 10*1024*1024 {
		log.Printf("[SEC] Подозрительно большой выходной файл: %d байт", fileInfo.Size())
		http.Error(w, "Результат слишком велик", http.StatusInternalServerError)
		return
	}

	w.Header().Set("Content-Type", "application/octet-stream")
	w.Header().Set("Content-Disposition", "attachment; filename=program")
	w.Header().Set("X-Content-Type-Options", "nosniff")

	file, err := os.Open(outputFile)
	if err != nil {
		log.Printf("[SEC] Не удалось открыть выходной файл: %v", err)
		http.Error(w, "Ошибка при отправке", http.StatusInternalServerError)
		return
	}
	defer file.Close()

	io.Copy(w, file)
}

func downloadCompilerHandler(w http.ResponseWriter, r *http.Request) {
	if _, err := os.Stat(FakePycPath); os.IsNotExist(err) {
		http.Error(w, "Компилятор недоступен", http.StatusServiceUnavailable)
		return
	}
	http.ServeFile(w, r, FakePycPath)
}

func homeHandler(w http.ResponseWriter, r *http.Request) {
	if r.URL.Path != "/" {
		http.NotFound(w, r)
		return
	}
	w.Header().Set("Content-Type", "text/html; charset=utf-8")
	w.Header().Set("X-Content-Type-Options", "nosniff")
	http.ServeFile(w, r, ViewHTML)
}

func main() {
	for _, f := range []string{ViewHTML, FakePycPath} {
		if _, err := os.Stat(f); os.IsNotExist(err) {
			log.Fatalf("❌ Файл не найден: %s", f)
		}
	}
	if err := exec.Command("test", "-x", FakePycPath).Run(); err != nil {
		log.Fatalf("❌ fakepyc не исполняемый")
	}

	server := &http.Server{
		Addr:         "0.0.0.0:8000",
		ReadTimeout:  5 * time.Second,
		WriteTimeout: 35 * time.Second,
		IdleTimeout:  60 * time.Second,
	}

	http.HandleFunc("/", homeHandler)
	http.HandleFunc("/compile", compileHandler)
	http.HandleFunc("/download-compiler", downloadCompilerHandler)

	fmt.Println("🚀 Сервер запущен на http://0.0.0.0:8000")
	log.Fatal(server.ListenAndServe())
}
