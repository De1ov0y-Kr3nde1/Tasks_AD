let read_file filename =
  let ic = open_in filename in
  let n = in_channel_length ic in
  let s = Bytes.create n in
  really_input ic s 0 n;
  close_in ic;
  Bytes.to_string s

let lines_of_string s =
  let lines = String.split_on_char '\n' s in
  List.map (fun line ->
      if String.length line > 0 && line.[String.length line - 1] = '\r' then
        String.sub line 0 (String.length line - 1)
      else
        line
    ) lines

let contains s substr =
  let len_s = String.length s in
  let len_sub = String.length substr in
  if len_sub = 0 then true
  else if len_sub > len_s then false
  else
    let max_start = len_s - len_sub in
    let rec loop i =
      if i > max_start then false
      else if String.sub s i len_sub = substr then true
      else loop (i + 1)
    in
    loop 0

let dangerous_patterns = [
  "open("; "open ("; ".read("; ".write("; "file=";
  "os."; "subprocess."; "sys."; "popen("; "spawn("; "fork(";
  "exec("; "eval("; "__import__("; "compile(";
  "import os"; "import subprocess"; "import sys";
  "ctypes"; "chown"; "chmod"; "setuid"; "setgid";
  "/root/"; "/etc/"; "/home/"; "/flag"; "flag.txt"; "Flag/"; "../"
]

let has_danger lines =
  let code = String.lowercase_ascii (String.concat "\n" lines) in
  List.exists (fun pat -> contains code (String.lowercase_ascii pat)) dangerous_patterns

(* Функция для извлечения тела функции с правильным порядком строк *)
let get_function_body func_name lines =
  let rec find_start idx =
    if idx >= List.length lines then []
    else if contains (List.nth lines idx) func_name then
      let start_idx = idx + 1 in
      let rec collect_body i indent acc =
        if i >= List.length lines then acc  (* Уже в правильном порядке, не нужно List.rev *)
        else
          let line = List.nth lines i in
          let trimmed = String.trim line in
          if trimmed = "" then collect_body (i + 1) indent acc
          else
            let line_indent = String.length line - String.length trimmed in
            if line_indent > indent || (line_indent = indent && trimmed <> "") then
              collect_body (i + 1) indent (acc @ [line])  (* Добавляем в конец *)
            else
              acc  (* Уже в правильном порядке *)
      in
      (* Находим отступ первой непустой строки после определения *)
      let rec find_first_nonempty j =
        if j >= List.length lines then 0
        else
          let line = List.nth lines j in
          let trimmed = String.trim line in
          if trimmed = "" then find_first_nonempty (j + 1)
          else String.length line - String.length trimmed
      in
      let first_indent = find_first_nonempty start_idx in
      if first_indent = 0 then [] else collect_body start_idx first_indent []
    else
      find_start (idx + 1)
  in
  find_start 0

let normalize s = String.trim s

let check_privileged lines =
  let code = String.concat "\n" lines in
  if not (contains code "def main():") then false
  else if not (contains code "def exp():") then false
  else (
    let main_body = get_function_body "def main():" lines in
    let exp_body = get_function_body "def exp():" lines in
    
    let normalized_main = List.map normalize main_body in
    let normalized_exp = List.map normalize exp_body in
    
    (* Проверяем main *)
    let main_ok = match normalized_main with 
     | [h] -> h = "print(\"Hi\")" 
     | _ -> false in
    
    (* Проверяем exp - порядок важен! *)
    let exp_ok = normalized_exp = ["print(\"Нельзя использовать эксплойт\")"; "return 0"] in
    
    main_ok && exp_ok
  )

let () =
  if Array.length Sys.argv < 2 then exit 1;
  let input_file = Sys.argv.(1) in
  let output = if Array.length Sys.argv >= 3 then Sys.argv.(2) else "a.out" in
  let code = read_file input_file in
  let lines = lines_of_string code in

  let privileged = check_privileged lines in

  if not privileged then (
    if has_danger lines then (
      print_endline "Нет прав";
      exit 1
    )
  );

  let script = "#!/usr/bin/env python3\n" ^ code ^ "\nif __name__ == '__main__':\n    main()\n" in
  let oc = open_out output in
  output_string oc script;
  close_out oc;
  Unix.chmod output 0o755