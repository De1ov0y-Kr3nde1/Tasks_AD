package main

import (
	"fmt"
	"io"
	"log"
	"net/http"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"time"
)

const (
	FakePycPath = "/home/strike17/task_AD/Compile/deploy/fakepyc"
	TempDir     = "/tmp/fakepyc_work"
	ViewHTML    = "/home/strike17/task_AD/Compile/deploy/templates/view.html"
)

func init() {
	os.MkdirAll(TempDir, 0755)
}

func compileHandler(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		http.Error(w, "Только POST", http.StatusMethodNotAllowed)
		return
	}

	body, err := io.ReadAll(r.Body)
	if err != nil {
		http.Error(w, "Ошибка чтения запроса", http.StatusBadRequest)
		return
	}
	defer r.Body.Close()

	code := string(body)
	if strings.TrimSpace(code) == "" {
		http.Error(w, "Код не может быть пустым", http.StatusBadRequest)
		return
	}

	ts := time.Now().UnixNano()
	inputFile := filepath.Join(TempDir, fmt.Sprintf("code_%d.py", ts))
	outputFile := filepath.Join(TempDir, fmt.Sprintf("program_%d", ts))

	if err := os.WriteFile(inputFile, []byte(code), 0644); err != nil {
		log.Printf("Ошибка записи входного файла: %v", err)
		http.Error(w, "Ошибка сервера", http.StatusInternalServerError)
		return
	}
	defer os.Remove(inputFile)

	cmd := exec.Command(FakePycPath, inputFile, outputFile)
	out, err := cmd.CombinedOutput()
	if err != nil {
		log.Printf("fakepyc ошибка: %v, вывод: %s", err, out)
		w.Header().Set("Content-Type", "text/plain; charset=utf-8")
		w.WriteHeader(http.StatusBadRequest)
		w.Write(out)
		return
	}

	if _, err := os.Stat(outputFile); os.IsNotExist(err) {
		log.Println("Выходной файл не создан")
		http.Error(w, "Компилятор не создал выходной файл", http.StatusInternalServerError)
		return
	}
	defer os.Remove(outputFile)

	w.Header().Set("Content-Type", "application/octet-stream")
	w.Header().Set("Content-Disposition", "attachment; filename=program")

	file, err := os.Open(outputFile)
	if err != nil {
		log.Printf("Ошибка открытия выходного файла: %v", err)
		http.Error(w, "Ошибка при отправке", http.StatusInternalServerError)
		return
	}
	defer file.Close()

	io.Copy(w, file)
}

func homeHandler(w http.ResponseWriter, r *http.Request) {
	if r.URL.Path != "/" {
		http.NotFound(w, r)
		return
	}
	w.Header().Set("Content-Type", "text/html; charset=utf-8")
	http.ServeFile(w, r, ViewHTML)
}

func main() {
	if _, err := os.Stat(ViewHTML); os.IsNotExist(err) {
		log.Fatalf("❌ view.html не найден: %s", ViewHTML)
	}
	if _, err := os.Stat(FakePycPath); os.IsNotExist(err) {
		log.Fatalf("❌ fakepyc не найден: %s", FakePycPath)
	}
	if err := exec.Command("test", "-x", FakePycPath).Run(); err != nil {
		log.Fatalf("❌ fakepyc не исполняемый: %s", FakePycPath)
	}

	http.HandleFunc("/", homeHandler)
	http.HandleFunc("/compile", compileHandler)

	fmt.Println("🚀 Сервер запущен на http://0.0.0.0:8000")
	fmt.Println("📄 Откройте: http://<ваш_IP>:8000")
	log.Fatal(http.ListenAndServe("0.0.0.0:8000", nil))
}
