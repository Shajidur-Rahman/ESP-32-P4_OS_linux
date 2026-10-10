#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "wifi_terminal.h"

static const char *TAG = "wifi_term";
static volatile int s_client_sock = -1;

void wifi_terminal_push_rx(uint8_t c);

int wifi_terminal_send_data(const char *data, int len)
{
    if (s_client_sock >= 0 && data && len > 0) {
        int sent = send(s_client_sock, data, len, MSG_DONTWAIT);
        if (sent < 0) {
            // Socket error or disconnected
            close(s_client_sock);
            s_client_sock = -1;
        }
        return sent;
    }
    return 0;
}

static void wifi_terminal_task(void *pvParameters)
{
    int server_sock = -1;
    struct sockaddr_in server_addr;

    server_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (server_sock < 0) {
        ESP_LOGE(TAG, "Failed to create socket: errno %d", errno);
        vTaskDelete(NULL);
        return;
    }

    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(WIFI_TERMINAL_PORT);

    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        ESP_LOGE(TAG, "Socket bind failed: errno %d", errno);
        close(server_sock);
        vTaskDelete(NULL);
        return;
    }

    if (listen(server_sock, 1) < 0) {
        ESP_LOGE(TAG, "Socket listen failed: errno %d", errno);
        close(server_sock);
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "=========================================================");
    ESP_LOGI(TAG, "  Wi-Fi Linux Terminal Server listening on port %d!", WIFI_TERMINAL_PORT);
    ESP_LOGI(TAG, "  Connect with: telnet <ESP32-P4-IP> %d", WIFI_TERMINAL_PORT);
    ESP_LOGI(TAG, "            or: nc <ESP32-P4-IP> %d", WIFI_TERMINAL_PORT);
    ESP_LOGI(TAG, "=========================================================");

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        int client = accept(server_sock, (struct sockaddr *)&client_addr, &addr_len);
        if (client < 0) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
        ESP_LOGI(TAG, "Client connected to Linux shell from %s:%d", client_ip, ntohs(client_addr.sin_port));

        if (s_client_sock >= 0) {
            close(s_client_sock);
        }
        s_client_sock = client;

        // Welcome banner over telnet
        const char *welcome = "\r\n*** Welcome to ESP32-P4 NOMMU Linux over Wi-Fi! ***\r\n\r\n";
        send(client, welcome, strlen(welcome), 0);

        // Read incoming keystrokes and inject into the Linux UART RX ring
        uint8_t rx_buf[128];
        while (s_client_sock == client) {
            int len = recv(client, rx_buf, sizeof(rx_buf), 0);
            if (len <= 0) {
                ESP_LOGI(TAG, "Client disconnected");
                close(client);
                if (s_client_sock == client) {
                    s_client_sock = -1;
                }
                break;
            }
            for (int i = 0; i < len; i++) {
                wifi_terminal_push_rx(rx_buf[i]);
            }
        }
    }
}

void wifi_terminal_start(void)
{
    xTaskCreate(wifi_terminal_task, "wifi_term", 4096, NULL, 5, NULL);
}
