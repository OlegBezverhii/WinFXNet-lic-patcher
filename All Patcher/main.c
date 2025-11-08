#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

// Структура для хранения позиций полей
typedef struct {
    int distributor_pos;
    int distributor_len;
    int company_pos;
    int company_len;
    int username_pos;
    int username_len;
    int license_pos;
    int license_len;
    int date_pos;
    int date_len;
} field_positions_t;

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

// Функция проверки контрольной суммы
int verify_checksum(unsigned char* data) {
    for (int i = 0; i < 128; i++) {
        unsigned char sum = data[i] + data[i + 128] + data[i + 256];
        if (sum != data[i + 384]) {
            return 0;
        }
    }
    return 1;
}

// Функция вычисления контрольной суммы
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

// Улучшенная функция для отображения данных с учетом специальных символов
void print_hex_with_ascii(const unsigned char* data, int start, int length) {
    printf("HEX: ");
    for (int i = start; i < start + length && i < start + 126; i++) {
        printf("%02X ", data[i]);
    }
    printf("\n");

    printf("TEXT: ");
    for (int i = start; i < start + length && i < start + 126; i++) {
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
    }
    printf("\n");
}

// Автоматическое определение позиций полей (исправленная логика)
int detect_field_positions(unsigned char* decrypted_data, field_positions_t* positions) {
    printf("=== Анализ структуры данных ===\n");
    print_hex_with_ascii(decrypted_data, 130, 126);

    // 1. Дистрибьютор (все до цифры 2)
    int distributor_end = -1;
    for (int i = 130; i < 256; i++) {
        if (decrypted_data[i] == '2') {
            distributor_end = i;
            break;
        }
    }

    if (distributor_end != -1) {
        positions->distributor_pos = 130;
        positions->distributor_len = distributor_end - 130;
        printf("Дистрибьютор: позиция %d, длина %d\n",
               positions->distributor_pos - 130, positions->distributor_len);
    }

    // 2. Компания (начинается с цифры 2, до \r\n)
    if (distributor_end != -1) {
        positions->company_pos = distributor_end;

        // Ищем конец компании (\r\n)
        int company_end = -1;
        for (int i = distributor_end; i < 256 - 1; i++) {
            if (decrypted_data[i] == 0x0D && decrypted_data[i+1] == 0x0A) {
                company_end = i;
                break;
            }
        }

        if (company_end != -1) {
            positions->company_len = company_end - distributor_end;
            printf("Компания: позиция %d, длина %d\n",
                   positions->company_pos - 130, positions->company_len);

            // 3. Имя пользователя (после \r\n)
            positions->username_pos = company_end + 2; // после \r\n

            // Ищем конец имени пользователя (до 0x00 или до 0x0E)
            int username_end = -1;
            for (int i = positions->username_pos; i < 256; i++) {
                if (decrypted_data[i] == 0x00 || decrypted_data[i] == 0x0E) {
                    username_end = i;
                    break;
                }
            }

            if (username_end != -1) {
                positions->username_len = username_end - positions->username_pos;
                printf("Пользователь: позиция %d, длина %d\n",
                       positions->username_pos - 130, positions->username_len);
            }
        }
    }

    // 4. Номер лицензии (после 0x0E, начинается с F01-)
    for (int i = 130; i < 256 - 14; i++) {
        if (decrypted_data[i] == 0x0E) {
            // Нашли 0x0E, проверяем что после него идет F01-
            if (i + 4 < 256 &&
                decrypted_data[i + 1] == 'F' &&
                decrypted_data[i + 2] == '0' &&
                decrypted_data[i + 3] == '1' &&
                decrypted_data[i + 4] == '-') {
                positions->license_pos = i + 1; // начинается с F
                positions->license_len = 14; // F01-XXXX-XXXXX
                printf("Лицензия: позиция %d, длина %d\n",
                       positions->license_pos - 130, positions->license_len);

                // 5. Дата (8 цифр после лицензии + 0x08)
                int date_search_pos = positions->license_pos + positions->license_len;
                if (date_search_pos < 256 && decrypted_data[date_search_pos] == 0x08) {
                    date_search_pos++; // пропускаем 0x08
                }

                // Ищем 8 цифр
                for (int j = date_search_pos; j < 256 - 8; j++) {
                    int is_date = 1;
                    for (int k = 0; k < 8; k++) {
                        if (decrypted_data[j + k] < '0' || decrypted_data[j + k] > '9') {
                            is_date = 0;
                            break;
                        }
                    }
                    if (is_date) {
                        positions->date_pos = j;
                        positions->date_len = 8;
                        printf("Дата: позиция %d, длина %d\n",
                               positions->date_pos - 130, positions->date_len);
                        break;
                    }
                }
                break;
            }
        }
    }

    return 1;
}

// Функция для очистки области данных
void clear_data_area(unsigned char* data, int start, int length) {
    for (int i = start; i < start + length && i < 256; i++) {
        data[i] = 0x00;
    }
}

// Функция патчинга с поддержкой всех полей
void patch_file_comprehensive(const char* input_file, const char* output_file,
                              const char* new_company, const char* new_username,
                              const char* new_license, const char* new_date) {
    unsigned char* file_data = NULL;
    long file_size = 0;

    printf("Комплексный патчинг файла: %s -> %s\n", input_file, output_file);

    if (!read_file(input_file, &file_data, &file_size)) {
        printf("Ошибка чтения файла\n");
        return;
    }

    if (file_size < 512) {
        printf("Файл слишком маленький\n");
        free(file_data);
        return;
    }

    // Проверяем исходную контрольную сумму
    printf("Проверка контрольной суммы: ");
    if (verify_checksum(file_data)) {
        printf("OK\n");
    } else {
        printf("НЕСОВПАДЕНИЕ\n");
    }

    // Создаем рабочую копию
    unsigned char* work_data = (unsigned char*)malloc(file_size);
    memcpy(work_data, file_data, file_size);
    decrypt_data(work_data);

    // Определяем позиции полей
    field_positions_t positions = {0};
    detect_field_positions(work_data, &positions);

    // Применяем изменения
    printf("\n=== Применение изменений ===\n");

    // Очищаем область данных от компании до даты
    if (positions.company_pos > 0) {
        int clear_start = positions.company_pos;
        int clear_end = (positions.date_pos > 0) ? positions.date_pos + positions.date_len : 256;
        clear_data_area(work_data, clear_start, clear_end - clear_start);
    }

    int current_pos = positions.company_pos;

    if (new_company && positions.company_pos > 0) {
        // Добавляем цифру '2' в начало названия компании
        char company_with_prefix[256];
        if (new_company[0] != '2') {
            snprintf(company_with_prefix, sizeof(company_with_prefix), "2%s", new_company);
        } else {
            strncpy(company_with_prefix, new_company, sizeof(company_with_prefix) - 1);
        }

        printf("Компания: '%s'\n", company_with_prefix);

        // Копируем компанию
        int copy_len = strlen(company_with_prefix);
        memcpy(&work_data[current_pos], company_with_prefix, copy_len);
        current_pos += copy_len;

        // Добавляем \r\n после компании
        work_data[current_pos++] = 0x0D;
        work_data[current_pos++] = 0x0A;
    }

    if (new_username && positions.username_pos > 0) {
        printf("Пользователь: '%s'\n", new_username);

        // Копируем имя пользователя
        int copy_len = strlen(new_username);
        memcpy(&work_data[current_pos], new_username, copy_len);
        current_pos += copy_len;

        // Добавляем нулевые байты после имени пользователя
        // Рассчитываем сколько нужно нулевых байтов до 0x0E
        int target_pos = positions.license_pos - 1; // позиция перед 0x0E
        int null_bytes_needed = target_pos - current_pos;

        printf("Добавляем %d нулевых байтов после имени пользователя\n", null_bytes_needed);

        for (int i = 0; i < null_bytes_needed && current_pos < 256; i++) {
            work_data[current_pos++] = 0x00;
        }
    }

    // Добавляем 0x0E перед номером лицензии
    if (positions.license_pos > 0) {
        work_data[current_pos++] = 0x0E;
    }

    if (new_license && positions.license_pos > 0) {
        printf("Лицензия: '%s'\n", new_license);

        int copy_len = strlen(new_license);
        if (copy_len > positions.license_len) copy_len = positions.license_len;
        memcpy(&work_data[current_pos], new_license, copy_len);
        current_pos += copy_len;

        // Добавляем 0x08 после лицензии
        work_data[current_pos++] = 0x08;
    }

    if (new_date && positions.date_pos > 0) {
        printf("Дата: '%s'\n", new_date);
        memcpy(&work_data[current_pos], new_date, positions.date_len);
        current_pos += positions.date_len;

        // Добавляем 19 нулевых байтов после даты
        printf("Добавляем 19 нулевых байтов после даты\n");
        for (int i = 0; i < 19 && current_pos < 256; i++) {
            work_data[current_pos++] = 0x00;
        }
    }

    // Заполняем оставшееся пространство нулями
    while (current_pos < 256) {
        work_data[current_pos++] = 0x00;
    }

    // Шифруем обратно
    encrypt_data(work_data);

    // Копируем обратно и обновляем контрольные суммы
    memcpy(file_data + 130, work_data + 130, 126);

    if (file_size >= 384) {
        memcpy(file_data + 256, work_data + 130, 126);
        calculate_checksum(file_data);
        printf("Контрольная сумма пересчитана\n");
    }

    // Сохраняем файл
    if (write_file(output_file, file_data, file_size)) {
        printf("\nФайл сохранен: %s\n", output_file);

        // Проверяем результат
        printf("\n=== Проверка результата ===\n");
        unsigned char* test_data = NULL;
        long test_size = 0;
        if (read_file(output_file, &test_data, &test_size)) {
            if (verify_checksum(test_data)) {
                printf("✓ Контрольная сумма верна\n");
            }

            decrypt_data(test_data);
            print_hex_with_ascii(test_data, 130, 126);

            // Подсчитываем нулевые байты после даты
            int null_count = 0;
            if (positions.date_pos > 0) {
                int date_end = positions.date_pos + positions.date_len;
                for (int i = date_end; i < 256 && work_data[i] == 0x00; i++) {
                    null_count++;
                }
            }
            printf("Нулевых байтов после даты: %d\n", null_count);

            free(test_data);
        }
    }

    free(file_data);
    free(work_data);
}

// Функция для интерактивного режима
void interactive_patch() {
    char input_file[256];
    char output_file[256];
    char company[256];
    char username[256];
    char license[256];
    char date[256];

    printf("Интерактивный режим патчинга\n");
    printf("============================\n");

    printf("Входной файл: ");
    scanf("%255s", input_file);

    printf("Выходной файл: ");
    scanf("%255s", output_file);

    printf("\nВведите новые значения (или Enter для пропуска):\n");

    printf("Компания (без цифры 2): ");
    getchar();
    fgets(company, sizeof(company), stdin);
    company[strcspn(company, "\n")] = 0;

    printf("Имя пользователя: ");
    fgets(username, sizeof(username), stdin);
    username[strcspn(username, "\n")] = 0;

    printf("Номер лицензии: ");
    fgets(license, sizeof(license), stdin);
    license[strcspn(license, "\n")] = 0;

    printf("Дата (YYYYMMDD): ");
    fgets(date, sizeof(date), stdin);
    date[strcspn(date, "\n")] = 0;

    printf("\n");

    patch_file_comprehensive(input_file, output_file,
                             company[0] ? company : NULL,
                             username[0] ? username : NULL,
                             license[0] ? license : NULL,
                             date[0] ? date : NULL);
}

int main(int argc, char* argv[]) {
    system("chcp 65001");
    printf("Расширенный патчер файлов лицензий\n");
    printf("==================================\n\n");

    if (argc == 1) {
        interactive_patch();
    } else if (argc >= 3) {
        const char* company = (argc > 3) ? argv[3] : NULL;
        const char* username = (argc > 4) ? argv[4] : NULL;
        const char* license = (argc > 5) ? argv[5] : NULL;
        const char* date = (argc > 6) ? argv[6] : NULL;

        patch_file_comprehensive(argv[1], argv[2], company, username, license, date);
    } else {
        printf("Использование:\n");
        printf("  Интерактивный режим: %s\n", argv[0]);
        printf("  Командный режим: %s input.bin output.bin [company] [username] [license] [date]\n", argv[0]);
    }

    return 0;
}