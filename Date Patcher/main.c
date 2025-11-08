#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

// Функция дешифровки
void decrypt_data(unsigned char* data) {
    unsigned char index = data[128];
    unsigned char* current_byte = &data[130];
    unsigned char counter = 126;

    do {
        unsigned char key_byte = data[index];
        unsigned char inverted_key = ~key_byte;
        *current_byte -= inverted_key;
        index = (index + 1) & 0x7F;
        current_byte++;
        counter--;
    } while (counter);
}

// Функция шифровки
void encrypt_data(unsigned char* data) {
    unsigned char index = data[128];
    unsigned char* current_byte = &data[130];
    unsigned char counter = 126;

    do {
        unsigned char key_byte = data[index];
        unsigned char inverted_key = ~key_byte;
        *current_byte += inverted_key;
        index = (index + 1) & 0x7F;
        current_byte++;
        counter--;
    } while (counter);
}

// Функция проверки контрольной суммы (как в ассемблерном коде)
int verify_checksum(unsigned char* data) {
    // Блоки по 128 байт:
    // blockA: data[0]   - data[127]   (ключевая строка)
    // blockB: data[128] - data[255]   (данные + что-то еще)
    // blockC: data[256] - data[383]   (расшифрованные данные?)
    // blockD: data[384] - data[511]   (контрольная сумма)

    for (int i = 0; i < 128; i++) {
        unsigned char sum = data[i] + data[i + 128] + data[i + 256];
        if (sum != data[i + 384]) {
            return 0; // Не совпадает
        }
    }
    return 1; // Все совпадает
}

// Функция вычисления контрольной суммы для блока D
void calculate_checksum(unsigned char* data) {
    for (int i = 0; i < 128; i++) {
        data[i + 384] = data[i] + data[i + 128] + data[i + 256];
    }
}

// Функция для чтения файла
int read_file(const char* filename, unsigned char** buffer, long* file_size) {
    FILE* file = fopen(filename, "rb");
    if (!file) return 0;

    fseek(file, 0, SEEK_END);
    *file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    *buffer = (unsigned char*)malloc(*file_size);
    if (!*buffer) {
        fclose(file);
        return 0;
    }

    fread(*buffer, 1, *file_size, file);
    fclose(file);
    return 1;
}

// Функция для записи файла
int write_file(const char* filename, unsigned char* buffer, long file_size) {
    FILE* file = fopen(filename, "wb");
    if (!file) return 0;

    fwrite(buffer, 1, file_size, file);
    fclose(file);
    return 1;
}

// Автоматический поиск и замена даты
void patch_file_with_checksum(const char* input_file, const char* output_file) {
    unsigned char* file_data = NULL;
    long file_size = 0;

    printf("Патчинг файла с проверкой контрольной суммы: %s\n", input_file);

    if (!read_file(input_file, &file_data, &file_size)) {
        printf("Ошибка чтения файла\n");
        return;
    }

    if (file_size < 512) {
        printf("Файл слишком маленький. Нужно минимум 512 байт для контрольных сумм\n");
        free(file_data);
        return;
    }

    printf("Размер файла: %ld байт\n", file_size);

    // Проверяем исходную контрольную сумму
    printf("Проверка исходной контрольной суммы: ");
    if (verify_checksum(file_data)) {
        printf("OK\n");
    } else {
        printf("НЕСОВПАДЕНИЕ! Файл поврежден или структура другая\n");
    }

    // Создаем рабочую копию для дешифровки
    unsigned char* work_data = (unsigned char*)malloc(file_size);
    memcpy(work_data, file_data, file_size);

    // ДЕШИФРУЕМ данные чтобы найти и изменить дату
    decrypt_data(work_data);

    // Ищем паттерн даты (8 цифр) в расшифрованных данных
    int date_start = -1;
    for (int i = 130; i < 130 + 126 - 8; i++) {
        // Проверяем 8 цифр подряд
        int is_date = 1;
        for (int j = 0; j < 8; j++) {
            if (work_data[i + j] < '0' || work_data[i + j] > '9') {
                is_date = 0;
                break;
            }
        }

        if (is_date) {
            date_start = i;
            printf("Найдена дата на позиции %d: ", date_start - 130);
            for (int j = 0; j < 8; j++) {
                printf("%c", work_data[date_start + j]);
            }
            printf("\n");
            break;
        }
    }

    if (date_start == -1) {
        printf("Дата не найдена!\n");
        free(file_data);
        free(work_data);
        return;
    }

    // Заменяем на новую дату
    char old_date[9] = {0};
    char new_date[] = "20500121";

    memcpy(old_date, &work_data[date_start], 8);
    printf("Замена даты: %s -> %s\n", old_date, new_date);

    for (int i = 0; i < 8; i++) {
        work_data[date_start + i] = new_date[i];
    }

    // Шифруем обратно с измененной датой
    encrypt_data(work_data);

    // Копируем измененные зашифрованные данные обратно
    memcpy(file_data + 130, work_data + 130, 126);

    // Теперь нужно обновить контрольную сумму в блоке D
    // Для этого копируем измененные данные в блок C (256-383)
    if (file_size >= 384) {
        memcpy(file_data + 256, work_data + 130, 126);
        // Остальные байты блока C (382-383) оставляем как были

        // Пересчитываем контрольную сумму для блока D
        calculate_checksum(file_data);
        printf("Контрольная сумма пересчитана\n");
    }

    // Проверяем новую контрольную сумму
    printf("Проверка новой контрольной суммы: ");
    if (verify_checksum(file_data)) {
        printf("OK\n");
    } else {
        printf("ОШИБКА! Контрольная сумма не совпадает\n");
    }

    // Сохраняем патченный файл
    if (write_file(output_file, file_data, file_size)) {
        printf("Патченный файл сохранен: %s\n", output_file);

        // Проверяем что патчинг сработал
        printf("\nПроверка патченного файла:\n");
        unsigned char* test_data = NULL;
        long test_size = 0;
        if (read_file(output_file, &test_data, &test_size)) {
            // Проверяем контрольную сумму
            if (verify_checksum(test_data)) {
                printf("✓ Контрольная сумма верна\n");
            } else {
                printf("✗ Контрольная сумма неверна!\n");
            }

            // Дешифруем и показываем данные
            decrypt_data(test_data);
            char test_string[127];
            for (int i = 0; i < 126; i++) {
                if (test_data[130 + i] >= 32 && test_data[130 + i] <= 126) {
                    test_string[i] = test_data[130 + i];
                } else {
                    test_string[i] = '.';
                }
            }
            test_string[126] = '\0';
            printf("Новые данные: %s\n", test_string);
            free(test_data);
        }
    } else {
        printf("Ошибка сохранения файла\n");
    }

    free(file_data);
    free(work_data);
}

// Функция для анализа структуры файла
void analyze_file_structure(const char* filename) {
    unsigned char* file_data = NULL;
    long file_size = 0;

    printf("Анализ структуры файла: %s\n", filename);

    if (!read_file(filename, &file_data, &file_size)) {
        return;
    }

    printf("Размер файла: %ld байт\n", file_size);
    printf("Блоки:\n");
    printf("  Block A (0-127):   ключевая строка\n");
    printf("  Block B (128-255): данные (байт 128=%02X, 129=%02X)\n",
           file_data[128], file_data[129]);

    if (file_size >= 384) {
        printf("  Block C (256-383): данные?\n");
    }
    if (file_size >= 512) {
        printf("  Block D (384-511): контрольная сумма\n");

        // Показываем несколько примеров контрольной суммы
        printf("  Примеры контрольных сумм:\n");
        for (int i = 0; i < 5; i++) {
            unsigned char calculated = file_data[i] + file_data[i + 128] + file_data[i + 256];
            printf("    [%d]: %02X + %02X + %02X = %02X (файл: %02X) %s\n",
                   i, file_data[i], file_data[i + 128], file_data[i + 256],
                   calculated, file_data[i + 384],
                   (calculated == file_data[i + 384]) ? "OK" : "MISMATCH");
        }
    }

    free(file_data);
}

int main() {
    system("chcp 65001");
    printf("Патчер файлов лицензий с контрольной суммой\n");
    printf("===========================================\n\n");

    // Анализируем структуру исходного файла
    analyze_file_structure("WinFXNet.lic");
    printf("\n");

    // Патчим файл
    patch_file_with_checksum("WinFXNet.lic", "WinFXNet_patched.lic");

    return 0;
}