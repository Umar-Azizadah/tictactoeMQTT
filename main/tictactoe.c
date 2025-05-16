#include <stdio.h>
#include <stdlib.h>
#include "mqtt_client.h"
#include "esp_log.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_task_wdt.h"
#include "driver/uart.h"

// Game modes
typedef enum {
    MODE_MENU,
    MODE_ONE_PLAYER,
    MODE_TWO_PLAYER,
    MODE_AI_PLAYERS
} game_mode_t;

char board[3][3];
char currentPlayer = 'X';
bool wifi_connected = false;
bool mqtt_connected = false;
bool game_started = false;
game_mode_t current_mode = MODE_MENU;

static esp_mqtt_client_handle_t client = NULL;
static const char *TAG = "TicTacToe";

// WiFi config
#define WIFI_SSID "Linksys03130"
#define WIFI_PASS "0c2fzyk6dv"

// UART config for console input
#define UART_NUM UART_NUM_0
#define BUF_SIZE (1024)

void initializeBoard();
void printBoard();
void getPlayerMove();
int checkWinner();
void connect_wifi();
void mqtt_app_start();
void uart_task(void *pvParameters);
void display_menu();
void handle_menu_selection(int selection);
void start_one_player_mode();
void start_two_player_mode();
void start_automate_play_mode();
void process_player_move(int row, int col);

// publish ready status
void send_ready(const char* msg) {
    if (mqtt_connected) {
        esp_mqtt_client_publish(client, "tictactoe/ready", msg, 0, 1, 0);
        ESP_LOGI(TAG, "Published to ready topic: %s", msg);
    }
}

// mqtt event callback
static esp_err_t mqtt_event_handler_cb(esp_mqtt_event_handle_t event) {
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT Connected");
            mqtt_connected = true;
            esp_mqtt_client_subscribe(client, "tictactoe/control", 0);
            
            // If we're in the menu mode, display the menu
            if (current_mode == MODE_MENU) {
                display_menu();
            }
            // If we're in one-player mode and game not started, start it
            else if (current_mode == MODE_ONE_PLAYER && !game_started) {
                start_one_player_mode();
            }
            else if (current_mode == MODE_AI_PLAYERS && !game_started) {
                start_automate_play_mode();
            }
            break;
            
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT Disconnected");
            mqtt_connected = false;
            break;
            
        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MQTT Data Received");

            // Only process MQTT data in one-player mode and when it's Player O's turn
            if (current_mode == MODE_ONE_PLAYER && currentPlayer == 'O') {
                char data[32] = {0};
                snprintf(data, sizeof(data), "%.*s", event->data_len, event->data);
                char player;
                int row, col;
                
                if (sscanf(data, "%c,%d,%d", &player, &row, &col) == 3 && player == 'O') {
                    ESP_LOGI(TAG, "Received move: %c %d %d", player, row, col);

                    if (row >= 0 && row < 3 && col >= 0 && col < 3 && board[row][col] == ' ') {
                        board[row][col] = currentPlayer;
                        printBoard();  // Print board after MQTT player's move
                        
                        int winner = checkWinner();
                        if (winner) {
                            if (winner == 3) {
                                printf("It's a draw!\n");
                            } else {
                                printf("Player %c wins!\n", winner == 1 ? 'X' : 'O');
                            }
                            send_ready("done");
                            // Game is over, but keep the program running
                            // Return to menu after a brief delay
                            vTaskDelay(3000 / portTICK_PERIOD_MS);
                            game_started = false;
                            current_mode = MODE_MENU;
                            display_menu();
                        } else {
                            currentPlayer = 'X';
                            printf("Human Player's turn (X)\n");
                            printf("Enter row and column (0-2): ");
                            fflush(stdout);
                            send_ready("next");
                        }
                    } else {
                        send_ready("taken");
                    }
                }
            }
            break;
            
        default:
            break;
    }
    return ESP_OK;
}

// mqtt wrapper
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    mqtt_event_handler_cb(event_data);
}

// WiFi event handler
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                              int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT) {
        if (event_id == WIFI_EVENT_STA_START) {
            esp_wifi_connect();
            ESP_LOGI(TAG, "WiFi connecting...");
        } else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
            wifi_connected = false;
            ESP_LOGI(TAG, "WiFi lost connection, attempting to reconnect...");
            esp_wifi_connect();
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "WiFi connected! IP:" IPSTR, IP2STR(&event->ip_info.ip));
        wifi_connected = true;
        
        // Start MQTT after WiFi is connected
        mqtt_app_start();
    }
}

// Display the main menu
void display_menu() {
    printf("\n\n=== ESP32 Tic-Tac-Toe ===\n");
    printf("Select game mode:\n");
    printf("1. One Player (vs. MQTT/Bash Script)\n");
    printf("2. Two Players (Human vs. Human)\n");
    printf("3. Automate Play\n");
    printf("\nEnter your choice (1-3): ");
    fflush(stdout);
}

// Handle menu selection
void handle_menu_selection(int selection) {
    switch (selection) {
        case 1:
            current_mode = MODE_ONE_PLAYER;
            if (mqtt_connected) {
                start_one_player_mode();
            } else {
                printf("Waiting for MQTT connection to start game...\n");
            }
            break;
        case 2:
            current_mode = MODE_TWO_PLAYER;
            start_two_player_mode();
            break;
        case 3:
            current_mode = MODE_AI_PLAYERS;
            start_automate_play_mode();
            break;
        default:
            printf("Invalid selection. Please try again.\n");
            display_menu();
            break;
    }
}

// Start one-player mode (vs. MQTT)
void start_one_player_mode() {
    game_started = true;
    currentPlayer = 'X';
    initializeBoard();
    send_ready("new");  // Signal that a new game is starting
    
    printf("\n=== One Player Mode ===\n");
    printf("Player X = Human (Serial input)\n");
    printf("Player O = Bash Script (MQTT input)\n\n");
    printBoard();  // Print initial empty board
    printf("Human Player's turn (X)\n");
    printf("Enter row and column (0-2): ");
    fflush(stdout);
}

// Start two-player mode (human vs. human)
void start_two_player_mode() {
    game_started = true;
    currentPlayer = 'X';
    initializeBoard();
    
    printf("\n=== Two Player Mode ===\n");
    printf("Player X and Player O both use serial input\n\n");
    printBoard();  // Print initial empty board
    printf("Player %c's turn\n", currentPlayer);
    printf("Enter row and column (0-2): ");
    fflush(stdout);
}

void start_automate_play_mode() {
    game_started = true;
    currentPlayer = 'X';
    initializeBoard();

    printf("\n=== AI vs AI Mode ===\n");
    printf("Player X (C program) Player O (bash script)\n\n");
    printBoard();
    printf("Player %c's turn\n", currentPlayer);
}

// Process a player's move
void process_player_move(int row, int col) {
    // Check if the move is valid
    if (row >= 0 && row < 3 && col >= 0 && col < 3 && board[row][col] == ' ') {
        board[row][col] = currentPlayer;
        printBoard();
        
        int winner = checkWinner();
        if (winner) {
            if (winner == 3) {
                printf("It's a draw!\n");
            } else {
                printf("Player %c wins!\n", winner == 1 ? 'X' : 'O');
            }
            
            // If in one-player mode, notify via MQTT
            if (current_mode == MODE_ONE_PLAYER && mqtt_connected) {
                send_ready("done");
            }
            
            // Return to menu after a delay
            vTaskDelay(3000 / portTICK_PERIOD_MS);
            game_started = false;
            current_mode = MODE_MENU;
            display_menu();
        } else {
            // Switch players
            currentPlayer = (currentPlayer == 'X') ? 'O' : 'X';
            
            // In one-player mode, notify MQTT if it's player O's turn
            if (current_mode == MODE_ONE_PLAYER) {
                if (currentPlayer == 'O') {
                    printf("Waiting for Player O's move via MQTT...\n");
                    send_ready("next");
                } else {
                    printf("Human Player's turn (X)\n");
                    printf("Enter row and column (0-2): ");
                    fflush(stdout);
                }
            } 
            //in two-player mode, prompt the next player
            else if (current_mode == MODE_TWO_PLAYER) {
                printf("Player %c's turn\n", currentPlayer);
                printf("Enter row and column (0-2): ");
                fflush(stdout);
            }
        }
    } else {
        printf("Invalid move. Spot taken or out of range.\n");
        printf("Enter row and column (0-2): ");
        fflush(stdout);
    }
}

//uART task to handle player input
void uart_task(void *pvParameters) {
    char input[16];
    int idx = 0;
    uint8_t data;
    
    while (1) {
        if (uart_read_bytes(UART_NUM, &data, 1, portMAX_DELAY) > 0) {
            //echo the received character
            uart_write_bytes(UART_NUM, (const char*)&data, 1);
            
            if (data == '\r' || data == '\n') {
                //process the input when Enter is pressed
                if (idx > 0) {
                    input[idx] = '\0';
                    idx = 0;
                    
                    //handle menu selection
                    if (current_mode == MODE_MENU) {
                        int selection = atoi(input);
                        handle_menu_selection(selection);
                    }
                    //handle game moves
                    else if (game_started) {
                        //in one-player mode, only process input when it's Player X's turn
                        if (current_mode == MODE_ONE_PLAYER && currentPlayer == 'X') {
                            int row, col;
                            if (sscanf(input, "%d %d", &row, &col) == 2) {
                                process_player_move(row, col);
                            } else {
                                printf("Invalid input. Format should be: row col\n");
                                printf("Enter row and column (0-2): ");
                                fflush(stdout);
                            }
                        }
                        //in two-player mode, process input for both players
                        else if (current_mode == MODE_TWO_PLAYER) {
                            int row, col;
                            if (sscanf(input, "%d %d", &row, &col) == 2) {
                                process_player_move(row, col);
                            } else {
                                printf("Invalid input. Format should be: row col\n");
                                printf("Enter row and column (0-2): ");
                                fflush(stdout);
                            }
                        }
                    }
                }
                //print a new line for better formatting
                printf("\n");
            } 
            else if (data == 127 || data == 8) {  // Backspace or Delete
                if (idx > 0) {
                    idx--;
                    //send backspace, space, backspace to visually erase the character
                    uart_write_bytes(UART_NUM, "\b \b", 3);
                }
            }
            else if (idx < sizeof(input) - 1) {
                input[idx++] = data;
            }
        }
    }
}

void app_main() {
    ESP_LOGI(TAG, "Initializing...");
    
    //initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    //initialize game board
    initializeBoard();
    
    //configure UART for input
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 122,
    };
    ESP_ERROR_CHECK(uart_param_config(UART_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM, BUF_SIZE * 2, 0, 0, NULL, 0));
    
    //create a task to handle UART input
    xTaskCreate(uart_task, "uart_task", 4096, NULL, 10, NULL);
    
    //connect to WiFi (MQTT will start once WiFi connects)
    printf("Connecting to WiFi...\n");
    connect_wifi();
    
    //display the menu while waiting for connections
    printf("Welcome to ESP32 Tic-Tac-Toe!\n");
    printf("Initializing...\n");
    
    //main loop just keeps the system running
    while (1) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

//connect wifi
void connect_wifi() {
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    //register event handlers
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    ESP_LOGI(TAG, "WiFi initialization completed");
}

//mqtt start
void mqtt_app_start() {
    ESP_LOGI(TAG, "Starting MQTT client...");
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = "mqtt://35.197.29.168",
    };

    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
}


void initializeBoard() {
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            board[i][j] = ' ';
}

void printBoard() {
    //board display
    printf("\nCurrent board:\n\n");
    for (int i = 0; i < 3; i++) {
        printf(" %c | %c | %c \n", board[i][0], board[i][1], board[i][2]);
        if (i < 2)
            printf("---+---+---\n");
    }
    printf("\n");
}

int checkWinner() {
    //check rows
    for (int i = 0; i < 3; i++) {
        if (board[i][0] == board[i][1] && board[i][1] == board[i][2] && board[i][0] != ' ')
            return (board[i][0] == 'X') ? 1 : 2;
    }
    
    //check columns
    for (int i = 0; i < 3; i++) {
        if (board[0][i] == board[1][i] && board[1][i] == board[2][i] && board[0][i] != ' ')
            return (board[0][i] == 'X') ? 1 : 2;
    }

    //check diagonals
    if (board[0][0] == board[1][1] && board[1][1] == board[2][2] && board[0][0] != ' ')
        return (board[0][0] == 'X') ? 1 : 2;
    if (board[0][2] == board[1][1] && board[1][1] == board[2][0] && board[0][2] != ' ')
        return (board[0][2] == 'X') ? 1 : 2;

    //check for draw
    int draw = 1;
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            if (board[i][j] == ' ')
                draw = 0;

    return draw ? 3 : 0;
}