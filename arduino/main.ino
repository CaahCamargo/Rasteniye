#include "dht.h" 
#include <Ethernet.h>
#include <SPI.h>
#include <MySQL_Connection.h>
#include <MySQL_Cursor.h>
#include <MySQL_Encrypt_Sha1.h> 
#include <chrono>
#include <ctime>

byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED }; // Endereço MAC da sua Ethernet Shield
IPAddress serverIP(54, 39, 75, 7); // Endereço IP do servidor MySQL
EthernetClient client;
MySQL_Connection conn((Client *)&client);
char user[] = "uyxdtln97bpedj4z"; // Nome de usuário do banco de dados
char password[] = "rGUUNhx4FnBq3kWn32W9"; // Senha do banco de dados
char dbname[] = "bppxebupxzuv1cqe1tw4"; // Nome do banco de dados

#define umidadeAnalogica A0 // Pino para leitura analógica do sensor de umidade do solo
#define umidadeDigital 7    // Pino para leitura digital do sensor de umidade do solo
#define umiTempAmb A1       // Pino para leitura do sensor de umidade e temperatura ambiente

int valorUmidade;          // Armazena o valor da umidade do solo (leitura analógica)
int valorUmidadeDigital;   // Armazena o valor da umidade do solo (leitura digital)
int porta_rele1 = 9;       // Rele da ventoinha
int porta_rele2 = 6;       // Rele da válvula solenóide
int umidade;
int eqtCul;
int temperatura;
int umidadeAmb;
int volumeAgua;

dht DHT; // Instância do sensor DHT

// Definição dos pinos do sensor de fluxo de água
const int INTERRUPCAO_SENSOR = 0; // Pino de interrupção do sensor de fluxo de água
const int PINO_SENSOR = 2;        // Pino do sensor de fluxo de água

unsigned long contador = 0;             // Contador de pulsos do sensor de fluxo de água
const float FATOR_CALIBRACAO = 4.5;    // Fator de calibração para conversão dos pulsos em litros
float fluxo = 0;                        // Taxa de fluxo de água (litros por minuto)
float volume = 0;                       // Volume de água (litros)
float volumeTotal = 0;                  // Volume total de água (litros)
unsigned long tempoAntes = 0;           // Tempo anterior para cálculos de fluxo e volume

void setup() {
  Ethernet.begin(mac);
  // Inicialização da comunicação serial com velocidade de 9600 bps
  Serial.begin(9600);

  // Conectar-se ao banco de dados
  if (conn.connect(serverIP, 3306, user, password)) {
    delay(1000);
  } else {
    Serial.println("Falha na conexão ao banco de dados");
    while (1);
  }

  // Reles elétricos
  pinMode(porta_rele1, OUTPUT);
  pinMode(porta_rele2, OUTPUT);
  digitalWrite(porta_rele1, HIGH);  //Desliga rele 1
  digitalWrite(porta_rele2, HIGH);  //Desliga rele 2

  // Configuração dos pinos como entradas
  pinMode(umidadeAnalogica, INPUT);
  pinMode(umidadeDigital, INPUT);

  // Inicialização do monitor serial para o sensor de fluxo de água
  Serial.begin(9600);

  // Configuração do pino do sensor como entrada com pull-up
  pinMode(PINO_SENSOR, INPUT_PULLUP);

  delay(2000);

  // Inicializa a conexão Ethernet e obtém um endereço IP usando DHCP
   // Conectar-se ao banco de dados
  if (conn.connect(serverIP, 3306, user, password)) {
    delay(1000);
  } else {
    Serial.println("Falha na conexão ao banco de dados");
    while (1);
  }

  // Imprime o endereço IP obtido
  Serial.print("Endereço IP: ");
  Serial.println(Ethernet.localIP());
}

void loop() {
  // Leitura da umidade do solo (sensor analógico)
  valorUmidade = analogRead(umidadeAnalogica);
  valorUmidade = map(valorUmidade, 1023, 315, 0, 100); // Escala de 0 a 100

  // Leitura da umidade do solo (sensor digital)
  valorUmidadeDigital = digitalRead(umidadeDigital);

  // Leitura da umidade e temperatura ambiente (sensor DHT)
  DHT.read11(umiTempAmb);
  
  // Leitura do sensor de fluxo de água e cálculos
  if ((millis() - tempoAntes) > 1000) { // Executa a cada segundo
    detachInterrupt(INTERRUPCAO_SENSOR); // Desabilita a interrupção temporariamente
    fluxo = ((1000.0 / (millis() - tempoAntes)) * contador) / FATOR_CALIBRACAO;
    volume = fluxo / 60;
    volumeTotal += volume;
    contador = 0; // Reinicializa o contador
    tempoAntes = millis();
    attachInterrupt(INTERRUPCAO_SENSOR, contadorPulso, FALLING); // Habilita a interrupção novamente
  }

  // Exibe os resultados no monitor serial
  Serial.println("===== Leituras =====");
  Serial.println("Umidade do Solo (Analógico): " + String(valorUmidade) + "%");
  if(valorUmidade < 65)
  {
    auto now = std::chrono::system_clock::now();
    std::time_t currentTime = std::chrono::system_clock::to_time_t(now);

    // Obtendo a estrutura de tempo local
    struct std::tm *localTime = std::localtime(&currentTime);

    // Extraindo informações de data e hora
    int ano = localTime->tm_year + 1900; // Adicionando 1900, pois tm_year contém o número de anos desde 1900
    int mes = localTime->tm_mon + 1;    // Adicionando 1, pois tm_mon é baseado em zero (0 = janeiro)
    int dia = localTime->tm_mday;
    int hora = localTime->tm_hour;
    int minuto = localTime->tm_min;
    int segundo = localTime->tm_sec;
    int numLeit = 5;
    char tipAcio = "I";

    std::string data = std::to_string(ano) + '-' + std::to_string(mes) + '-' + std::to_string(dia);
    std::string hora = std::to_string(hora) + ':' + std::to_string(minuto) + ':' + std::to_string(segundo);

    char INSERT_SQL1[300]; // Defina o tamanho da consulta SQL conforme necessário
    snprintf(INSERT_SQL1, sizeof(INSERT_SQL1), "INSERT INTO %s.acionamento (numLeit, datAcio, horAcio, tipAcio) VALUES (%d, %s, %s, %c)",
    dbname, numLeit, String(datAcio).c_str(), String(horAcio).c_str(), tipAcio.c_str());
  
    MySQL_Cursor *cur_mem = new MySQL_Cursor(&conn);
    cur_mem->execute(INSERT_SQL1);
    delete cur_mem;

    digitalWrite(porta_rele2, LOW); 
    delay(60000);
    digitalWrite(porta_rele2, HIGH); 
  }
  else
  {
    digitalWrite(porta_rele2, HIGH); 
  }
  Serial.println("Umidade do Solo (Digital): " + String(valorUmidadeDigital == 0 ? "Solo úmido" : "Solo seco"));
  Serial.println("Umidade Ambiente: " + String(DHT.humidity, 0) + "%");
  Serial.println("Temperatura Ambiente: " + String(DHT.temperature, 0) + "°C");
  if(DHT.temperature > 32)
  {
     digitalWrite(porta_rele1, LOW);  
  }
  else
  {
    digitalWrite(porta_rele1, HIGH);
  }
  Serial.println("Fluxo de Água: " + String(fluxo, 2) + " L/min");
  Serial.println("Volume de Água: " + String(volume, 2) + " L");
  Serial.println("Volume Total de Água: " + String(volumeTotal, 2) + " L");
  Serial.println("");

  umidade = String(valorUmidade);
  eqtCul = 1;
  temperatura = String(DHT.temperature, 0);
  umidadeAmb = String(DHT.humidity, 0);
  volumeAgua = String(volumeTotal, 2);

  char INSERT_SQL[300]; // Defina o tamanho da consulta SQL conforme necessário
  snprintf(INSERT_SQL, sizeof(INSERT_SQL), "INSERT INTO %s.leitura (numEqtCul, umiSolo, tempAmb, umiAmb, volAgua) VALUES (%d, %s, %s, %s, %s)",
         dbname, eqtCul, String(valorUmidade).c_str(), temperatura.c_str(), umidadeAmb.c_str(), volumeAgua.c_str());
  
  MySQL_Cursor *cur_mem = new MySQL_Cursor(&conn);
  cur_mem->execute(INSERT_SQL);
  delete cur_mem;

  // Aguarda 1 hora antes de repetir as leituras
  delay(3600000); // 1 hora em milissegundos
}

// Função chamada pela interrupção para contagem de pulsos do sensor de fluxo de água
void contadorPulso() {
  contador++;
}