let read_file filename =
  let ic = open_in filename in
  let n = in_channel_length ic in
  let s = Bytes.create n in
  really_input ic s 0 n;
  close_in ic;
  Bytes.to_string s

let lines_of_string s = String.split_on_char '\n' s

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

(* Только опасные операции: файлы и привилегии *)
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

let check_privileged lines =
  let code = String.concat "\n" lines in
  if not (contains code "def main():") then false
  else if not (contains code "def exp():") then false
  else (
    let rec get_body start lines =
      let rec skip = function
        | [] -> []
        | x :: xs -> if contains x start then xs else skip xs
      in
      let rest = skip lines in
      match rest with
      | [] -> []
      | h :: _ ->
          let base_ind = String.length h - String.length (String.trim h) in
          if base_ind = 0 then [] else
            let rec take acc = function
              | [] -> List.rev acc
              | y :: ys ->
                  let t = String.trim y in
                  if t = "" then take acc ys
                  else
                    let ind = String.length y - String.length t in
                    if ind > base_ind then take (y :: acc) ys
                    else List.rev acc
            in
            take [] rest
    in
    let main_b = List.map String.trim (get_body "def main():" lines) in
    let exp_b = List.map String.trim (get_body "def exp():" lines) in
    (match main_b with h :: _ -> h = "print(\"Hi\")" | _ -> false) &&
    (exp_b = ["print(\"Нельзя использовать эксплойт\")"; "return 0"])
  )

let () =
  if Array.length Sys.argv < 2 then exit 1;
  let input_file = Sys.argv.(1) in
  let output = if Array.length Sys.argv >= 3 then Sys.argv.(2) else "a.out" in

  let code = read_file input_file in
  let lines = lines_of_string code in

  let danger = has_danger lines in
  let privileged = if danger then check_privileged lines else true in

  if danger && not privileged then (
    print_string "Нет прав";
    exit 1
  );

  (* Генерируем исполняемый Python-скрипт без ограничений *)
  let script = "#!/usr/bin/env python3\n" ^ code ^ "\nif __name__ == '__main__':\n    main()\n" in
  let oc = open_out output in
  output_string oc script;
  close_out oc;
  Unix.chmod output 0o755