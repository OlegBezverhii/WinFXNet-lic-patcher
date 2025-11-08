//код из дипсика
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Функция преобразования (аналог ассемблерного кода)
void transform_data(unsigned char* data) {
    unsigned char index = data[128];  // начальный индекс из a2+128
    unsigned char* current_byte = &data[130];  // указатель на a2+130
    unsigned char counter = 126;

    do {
        // Получаем байт ключа из data[index]
        unsigned char key_byte = data[index];

        // Основная операция преобразования (как в ассемблере)
        unsigned char inverted_key = ~key_byte;  // not bl
        *current_byte -= inverted_key;           // sub byte ptr [eax],bl

        // Обновляем индекс (циклический 0-127)
        index = (index + 1) & 0x7F;

        // Переходим к следующему байту
        current_byte++;
        counter--;
    } while (counter);
}

// Функция для преобразования байтов в строку (игнорируя непечатаемые символы)
void bytes_to_string(const unsigned char* data, size_t start, size_t length, char* output) {
    printf("TEXT: ");
    size_t output_index = 0;
    for (size_t i = start; i < start + length && i < start + 126; i++)
    {
        //выводим строку в консоль с спецсимволами
        unsigned char c = data[i];
        if (c == 0x00) {
            printf("\\0");
        } else if (c == 0x0D) {
            printf("\\r");
        } else if (c == 0x0A) {
            printf("\\n");
        } else if (c == 0x0E) {
            printf("\\x0E");
        } else if (c == 0x08) {
            printf("\\b");
        } else if (c >= 32 && c <= 126) {
            printf("%c", c);
        } else {
            printf(".");
        }

        //формируем строку из массива символов
        if (data[i] >= 32 && data[i] <= 126) {  // печатаемые ASCII символы
            output[output_index++] = data[i];
        } else {
            output[output_index++] = '.';  // заменяем непечатаемые символы
        }
    }
    output[output_index] = '\0';  // завершаем строку
    printf("\n");
}

int main() {
    system("chcp 65001");
    const char* filename = "WinFXNet.lic";
    FILE* file = fopen(filename, "rb");

    if (!file) {
        printf("Ошибка: не удалось открыть файл %s\n", filename);
        return 1;
    }

    // Определяем размер файла
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    // Выделяем память для данных
    unsigned char* file_data = (unsigned char*)malloc(file_size);
    if (!file_data) {
        printf("Ошибка: не удалось выделить память\n");
        fclose(file);
        return 1;
    }

    // Читаем файл
    size_t bytes_read = fread(file_data, 1, file_size, file);
    fclose(file);

    if (bytes_read != file_size) {
        printf("Ошибка: не удалось прочитать весь файл\n");
        free(file_data);
        return 1;
    }

    // Проверяем, что данных достаточно
    if (file_size < 130 + 126) {
        printf("Ошибка: файл слишком маленький. Нужно минимум %d байт\n", 130 + 126);
        free(file_data);
        return 1;
    }

    printf("Размер файла: %ld байт\n\n", file_size);

    // Создаем копию данных для преобразования
    unsigned char* out_buff = (unsigned char*)malloc(file_size);
    if (!out_buff) {
        printf("Ошибка: не удалось выделить память для out_buff\n");
        free(file_data);
        return 1;
    }
    memcpy(out_buff, file_data, file_size);

    // Выводим оригинальные данные (первые 130+126 байт)
    printf("ОРИГИНАЛЬНЫЕ ДАННЫЕ (первые 256 байт):\n");
    printf("HEX: ");
    for (int i = 0; i < 256 && i < file_size; i++) {
        printf("%02X ", file_data[i]);
    }
    printf("\n");

    // Преобразуем данные
    printf("\nПреобразование данных...\n");
    transform_data(out_buff);

    // Выводим преобразованные данные
    printf("\nПРЕОБРАЗОВАННЫЕ ДАННЫЕ:\n");
    printf("HEX: ");
    for (int i = 130; i < 130 + 126 && i < file_size; i++) {
        printf("%02X ", out_buff[i]);
    }
    printf("\n");

    // Выводим как строку
    char string_output[127];
    bytes_to_string(out_buff, 130, 126, string_output);
    printf("Полученная строка:\n%s\n", string_output);

    // Очистка памяти
    free(file_data);
    free(out_buff);

    return 0;
}