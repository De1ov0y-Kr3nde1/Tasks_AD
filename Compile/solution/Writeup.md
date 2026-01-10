Запускаем сервис, заходим на сайт и видим среду для написания кода на Python, которая компилирует код и конвертирует его в ELF

![alt text](Pictures/image_0.png)

Понятно, что нам не интересно написание кода, нам нужен флаг, давайте скачаем компилятор и проанализируем его. Скачиваем его и заходим в IDA Pro, находим функцию основной логики

![alt text](Pictures/image_1.png)

Находим основную функцию проверки условий

![alt text](Pictures/image_2.png)

Переходим в функцию camlCompiler_check_privileged_555

![alt text](Pictures/image_3.png)

Анализируем её, смотрим, какие строки используются для проверок

![alt text](Pictures/image_4.png)

![alt text](Pictures/image_5.png)

![alt text](Pictures/image_6.png)

![alt text](Pictures/image_7.png)

Смотрим, где используется каждая строка через Ctrl+x и восстанавливаем структуру кода, которая нужна для выполнения всех проверок, не забываем про 4 пробела в функциях (отступы в Python)

![alt text](Pictures/image_8.png)

Теперь пробуем внедрить эксплойт

![alt text](Pictures/image_9.png)

Отлично, получаем ELF файл, давайте его запустим

![alt text](Pictures/image_10.png)

Получаем флаг

