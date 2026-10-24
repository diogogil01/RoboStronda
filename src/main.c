// RoboStronda: 2 motores e 3 botoes via Wi-Fi (ESP-IDF, sem Arduino)
#include <stdbool.h>
#include <string.h>

#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

#define WIFI_NOME  "RoboStronda"
#define WIFI_SENHA "Robo1234"
#define TEMPO_MOTOR_US 2000000ULL

// Canal A = roda esquerda (confira a posicao real no robo)
#define AIN1 GPIO_NUM_25
#define AIN2 GPIO_NUM_26
#define PWMA GPIO_NUM_33

// Canal B = roda direita (confira a posicao real no robo)
#define BIN1 GPIO_NUM_16
#define BIN2 GPIO_NUM_17
#define PWMB GPIO_NUM_18

#define STBY GPIO_NUM_27

static const char *TAG = "RoboStronda";
static esp_timer_handle_t timer_parada;

static const char PAGINA[] =
    "<!DOCTYPE html><html lang='pt-br'><head>"
    "<meta charset='UTF-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>RoboStronda</title>"
    "<style>body{background:#171717;color:white;font-family:Arial;"
    "text-align:center;padding:35px 15px}"
    "button{display:block;width:90%;max-width:320px;margin:15px auto;"
    "padding:22px;border:0;border-radius:12px;background:#16a34a;"
    "color:white;font-size:20px;font-weight:bold}"
    "button:disabled{opacity:.5}p{color:#ccc}</style></head><body>"
    "<h1>ROBOSTRONDA</h1><h3>Controle das rodas</h3>"
    "<button onclick=\"acionar('direita')\">RODA DIREITA</button>"
    "<button onclick=\"acionar('esquerda')\">RODA ESQUERDA</button>"
    "<button onclick=\"acionar('ambos')\">AMBOS</button>"
    "<p id='estado'>Pronto</p>"
    "<script>async function acionar(qual){"
    "const botoes=document.querySelectorAll('button');"
    "const estado=document.getElementById('estado');"
    "botoes.forEach(b=>b.disabled=true);"
    "try{const r=await fetch('/'+qual,{method:'POST'});"
    "if(!r.ok)throw Error();estado.textContent='Rodando por 2 segundos';}"
    "catch(e){estado.textContent='Falha de comunicacao';}"
    "setTimeout(()=>{estado.textContent='Pronto';"
    "botoes.forEach(b=>b.disabled=false);},2200);"
    "}</script></body></html>";

// Parada automatica ou de seguranca
static void parar_motores(void *arg)
{
    (void)arg;
    gpio_set_level(STBY, 0);
    gpio_set_level(PWMA, 0);
    gpio_set_level(PWMB, 0);
    gpio_set_level(AIN1, 0);
    gpio_set_level(AIN2, 0);
    gpio_set_level(BIN1, 0);
    gpio_set_level(BIN2, 0);
}

static esp_err_t girar_motores(bool esquerda, bool direita)
{
    // Cancela o comando anterior, se ainda estiver ativo.
    esp_timer_stop(timer_parada);
    parar_motores(NULL);

    // Sentido fixo: IN1 alto e IN2 baixo em cada canal.
    gpio_set_level(AIN1, esquerda ? 1 : 0);
    gpio_set_level(AIN2, 0);
    gpio_set_level(PWMA, esquerda ? 1 : 0);

    gpio_set_level(BIN1, direita ? 1 : 0);
    gpio_set_level(BIN2, 0);
    gpio_set_level(PWMB, direita ? 1 : 0);

    gpio_set_level(STBY, 1);
    esp_err_t resultado = esp_timer_start_once(timer_parada, TEMPO_MOTOR_US);
    if (resultado != ESP_OK) {
        parar_motores(NULL);
    }
    return resultado;
}

static esp_err_t pagina_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_send(req, PAGINA, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t comando_handler(httpd_req_t *req)
{
    bool esquerda = strcmp(req->uri, "/esquerda") == 0;
    bool direita = strcmp(req->uri, "/direita") == 0;
    bool ambos = strcmp(req->uri, "/ambos") == 0;

    if (!esquerda && !direita && !ambos) {
        return httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Comando invalido");
    }

    esp_err_t resultado = girar_motores(esquerda || ambos, direita || ambos);
    if (resultado != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Erro no timer");
    }

    ESP_LOGI(TAG, "Comando: %s", req->uri);
    return httpd_resp_sendstr(req, "OK");
}

static void iniciar_servidor(void)
{
    httpd_handle_t servidor = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    ESP_ERROR_CHECK(httpd_start(&servidor, &config));

    httpd_uri_t pagina = {
        .uri = "/", .method = HTTP_GET, .handler = pagina_handler
    };
    httpd_uri_t direita = {
        .uri = "/direita", .method = HTTP_POST, .handler = comando_handler
    };
    httpd_uri_t esquerda = {
        .uri = "/esquerda", .method = HTTP_POST, .handler = comando_handler
    };
    httpd_uri_t ambos = {
        .uri = "/ambos", .method = HTTP_POST, .handler = comando_handler
    };

    ESP_ERROR_CHECK(httpd_register_uri_handler(servidor, &pagina));
    ESP_ERROR_CHECK(httpd_register_uri_handler(servidor, &direita));
    ESP_ERROR_CHECK(httpd_register_uri_handler(servidor, &esquerda));
    ESP_ERROR_CHECK(httpd_register_uri_handler(servidor, &ambos));
}

static void iniciar_wifi(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t wifi_config_padrao = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_config_padrao));

    wifi_config_t wifi = {0};
    strncpy((char *)wifi.ap.ssid, WIFI_NOME, sizeof(wifi.ap.ssid) - 1);
    strncpy((char *)wifi.ap.password, WIFI_SENHA, sizeof(wifi.ap.password) - 1);
    wifi.ap.ssid_len = strlen(WIFI_NOME);
    wifi.ap.channel = 1;
    wifi.ap.max_connection = 2;
    wifi.ap.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Rede Wi-Fi: %s", WIFI_NOME);
    ESP_LOGI(TAG, "Abra no iPhone: http://192.168.4.1");
}

void app_main(void)
{
    gpio_config_t pinos = {
        .pin_bit_mask = (1ULL << AIN1) | (1ULL << AIN2) |
                        (1ULL << BIN1) | (1ULL << BIN2) |
                        (1ULL << PWMA) | (1ULL << PWMB) |
                        (1ULL << STBY),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    ESP_ERROR_CHECK(gpio_config(&pinos));
    parar_motores(NULL);

    esp_timer_create_args_t timer_config = {
        .callback = parar_motores,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "parada_2s"
    };
    ESP_ERROR_CHECK(esp_timer_create(&timer_config, &timer_parada));

    esp_err_t resultado = nvs_flash_init();
    if (resultado == ESP_ERR_NVS_NO_FREE_PAGES ||
        resultado == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        resultado = nvs_flash_init();
    }
    ESP_ERROR_CHECK(resultado);

    iniciar_wifi();
    iniciar_servidor();
    ESP_LOGI(TAG, "Controle pronto: esquerda, direita e ambos");
}
