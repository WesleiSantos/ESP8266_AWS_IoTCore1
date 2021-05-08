
#include "FS.h"
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>

#define       LedBoard   2                             // WIFI Module LED
#define       BUTTON     D3                            // NodeMCU Button

//Dados da rede wifi
const char* ssid = "LEALNET_Teste";
const char* password = "9966leal";

const long utcOffsetInSeconds = -10800;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds);


/* Tópicos */
//============================================================================
const char* AWS_endpoint = "a1u5p9d1s2ikgy-ats.iot.us-east-1.amazonaws.com"; //MQTT broker ip
const char* topic_status = "cmd/esp8266/house/lampada/atualizar_status";
const char* topic_tarifa = "cmd/esp8266/house/lampada/atualizar_tarifa";
const char * topic_temporizador = "cmd/esp8266/house/lampada/atualizar_temporizador";

/*variáveis globais*/
//============================================================================
#define BUFFER_LEN 256
long lastMsg = 0;
char msg[BUFFER_LEN];
int value = 0;
int lampada_Id = 1234;
int mesAux = 0;
int diaAux = 0;
int horasTotais = 0;
int horasAux = -1;
int temporizador = 0;
int quantidadeHorasTemp = 0;
int diaTemp = 0;
int horarioTemp = 0;
float tarifa = 0.3;
float potencia = 40.0;
//============================================================================


/*função que recebe a mensagem no tópico*/
void callback(char* topic, byte* payload, unsigned int length) {
  if (strcmp(topic, topic_status) == 0) { 
    DynamicJsonDocument doc1(1024); 
    deserializeJson(doc1, payload);
    int led = doc1["status"]; 
    if (led == 1) //Se LED apagado
    {
      publish(1, 0, 0, 0);
      digitalWrite(2, LOW);
      Serial.println("Led ligou");

    }
    else if (led == 0)  //Se LED acesso
    {
      publish(2, 0, 0, 0);
      digitalWrite(2, HIGH);
      Serial.println("Led apagou");
    }
    Serial.println();
  } else if (strcmp(topic, topic_tarifa) == 0) {
    DynamicJsonDocument doc2(1024);
    deserializeJson(doc2, payload);
    float tarifa_float = doc2["tarifa"];
    tarifa = ceilf(tarifa_float * 100) / 100;
    Serial.printf("%f", tarifa);
  } else if (strcmp(topic, topic_temporizador) == 0) {
    DynamicJsonDocument doc3(1024);
    deserializeJson(doc3, payload);
    //seta as variáveis globais
    temporizador = doc3["status"];
    diaTemp = doc3["dia"];
    horarioTemp = doc3["hora_inicio"];
    quantidadeHorasTemp = doc3["quantidade_tempo"];
    
    int lead = digitalRead(LedBoard);
    if (lead == LOW) {
      publish(1, 0, 0, 0);
    }
    if (lead == HIGH) {
      publish(2, 0, 0, 0);
    }
    Serial.printf("\nstatus = %d dia = %d hora inicio = %d quantidade de tempo = %d\n", temporizador, diaTemp, horarioTemp, quantidadeHorasTemp);
  }
}

WiFiClientSecure espClient;
PubSubClient client(AWS_endpoint, 8883, callback, espClient); //define a porta mqtt

/*
 * Função responsável por publicar uma mensagem em um tópico;
 * int num, responsavel por definir tipo de ação a ser realizada;
 * int mes, mes atual 
 * int dia, dia atual
 * int quantidadeHoras, quantidade de tempo que a lampada ficou ligada durante o dia
 */
void publish(int num, int mes, int dia, int quantidadeHoras) {
  /*Calculo das custos diarios*/ 
  //================================================
  float potenciaTotal = potencia * quantidadeHoras;
  float valor_tarifa = tarifa;
  float custoTotal = valor_tarifa * potenciaTotal;
  //===============================================
  if (num == 0) {
    snprintf (msg, BUFFER_LEN, "{\"lampada_Id\" : %d, \"mes\" : %d, \"dia\" : %d, \"quantidadeHoras\" : %d, \"potenciaTotal\" : %f, \"custoTotal\" : %f, \"valorTarifa\" : %f }", lampada_Id, mes, dia, quantidadeHoras, potenciaTotal, custoTotal, valor_tarifa);
    Serial.print("Publish message: ");
    Serial.println(msg);
    client.publish("dt/esp8266/house/lampada", msg);
  } else if (num == 2) {
    snprintf (msg, BUFFER_LEN, "{\"id\" : %d, \"status\" : %d,\"potencia\" : %f, \"temporizador\" : %d, \"quantidadeHorasTemp\" : %d, \"diaTemp\" : %d, \"horarioTemp\": %d }", lampada_Id, 1, potencia, temporizador, quantidadeHorasTemp, diaTemp, horarioTemp);
    Serial.print("Publish message: ");
    Serial.println(msg);
    client.publish("cmd/esp8266/house/lampada/res", msg);
  } else {
    snprintf (msg, BUFFER_LEN, "{\"id\" : %d, \"status\" : %d, \"potencia\" : %f, \"temporizador\" : %d, \"quantidadeHorasTemp\" : %d, \"diaTemp\" : %d, \"horarioTemp\": %d }", lampada_Id, 0, potencia, temporizador, quantidadeHorasTemp, diaTemp, horarioTemp);
    Serial.print("Publish message: ");
    Serial.println(msg);
    client.publish("cmd/esp8266/house/lampada/res", msg);
  }
  Serial.print("Heap: "); Serial.println(ESP.getFreeHeap());
}


void setup_wifi() {
  delay(10);
  //conexão com o wifi
  espClient.setBufferSizes(512, 512);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  timeClient.begin();
  while (!timeClient.update()) {
    timeClient.forceUpdate();
  }

  espClient.setX509Time(timeClient.getEpochTime());

}


void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("ESPthing")) {
      Serial.println("connected");
      // publica quando conectado
      snprintf (msg, BUFFER_LEN, "{\"id\" : %d, \"status\" : %d,\"potencia\" : %f, \"temporizador\" : %d, \"quantidadeHorasTemp\" : %d, \"diaTemp\" : %d, \"horarioTemp\": %d }", lampada_Id, 1, potencia, temporizador, quantidadeHorasTemp, diaTemp, horarioTemp);
      Serial.print("Publish message: ");
      Serial.println(msg);
      client.publish("cmd/esp8266/house/lampada/res", msg);
      //Inscrição nos tópicos
      client.subscribe("cmd/esp8266/house/lampada/atualizar_status");
      client.subscribe("cmd/esp8266/house/lampada/atualizar_tarifa");
      client.subscribe("cmd/esp8266/house/lampada/atualizar_temporizador");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");

      char buf[256];
      espClient.getLastSSLError(buf, 256);
      Serial.print("WiFiClientSecure SSL error: ");
      Serial.println(buf);

      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  pinMode(LedBoard, OUTPUT);
  //Mantém o led desligado quando inicia o dispositivo
  digitalWrite(LedBoard, HIGH);
  setup_wifi();
  delay(1000);
  if (!SPIFFS.begin()) {
    Serial.println("Failed to mount file system");
    return;
  }

  Serial.print("Heap: "); Serial.println(ESP.getFreeHeap());

  // carrega o certificado
  File cert = SPIFFS.open("/cert.der", "r");
  if (!cert) {
    Serial.println("Failed to open cert file");
  }
  else
    Serial.println("Success to open cert file");

  delay(1000);

  if (espClient.loadCertificate(cert))
    Serial.println("cert loaded");
  else
    Serial.println("cert not loaded");

  // carrega chave privada
  File private_key = SPIFFS.open("/private.der", "r"); 
  if (!private_key) {
    Serial.println("Failed to open private cert file");
  }
  else
    Serial.println("Success to open private cert file");

  delay(1000);

  if (espClient.loadPrivateKey(private_key))
    Serial.println("private key loaded");
  else
    Serial.println("private key not loaded");

  // carrega certificado CA
  File ca = SPIFFS.open("/ca.der", "r"); 
  if (!ca) {
    Serial.println("Failed to open ca ");
  }
  else
    Serial.println("Success to open ca");

  delay(1000);

  if (espClient.loadCACert(ca))
    Serial.println("ca loaded");
  else
    Serial.println("ca failed");

  Serial.print("Heap: "); Serial.println(ESP.getFreeHeap());
}

/*função responsável por resetar temporizador*/
void resetTemporizador() {
  temporizador = 0;
  quantidadeHorasTemp = 0;
  diaTemp = 0;
  horarioTemp = 0;
}
/*função responsável por resetar variáveis auxiliares*/
void resetVarAux() {
  horasAux = -1;
  horasTotais = 0;
  diaAux = 0;
  mesAux = 0;
}

void loop() {

  timeClient.update();
  
  //Pega o horário atual
  int horario = timeClient.getHours();
  int minutos = timeClient.getMinutes();
  int segundos = timeClient.getSeconds() + 1;
  int tempoSegundos = (minutos * 60) + segundos;
  
  /*
   * calculo de conversão de escala do horário
   * 1 mês = 2h
   * 1 dia = 10s
  */
  //=====================================================================================================================
  int mesCorreto = (horario + 2) / 2;
  int diaCorreto = (horario + 2) % 2 == 0 ? ceil(((float)tempoSegundos / 240)) : ceil(((float)tempoSegundos / 240) + 15);
  int horarioCorreto = (tempoSegundos / 10) % 24;
  //=====================================================================================================================

  Serial.printf("%d/%d : %dh \n ", diaCorreto, mesCorreto, horarioCorreto);

  delay(1000);

  if (!client.connected()) {
    reconnect();
  }
  //verifica se é o fim do dia para fazer o envio dos dados para o banco
  if (diaAux != diaCorreto && diaAux != 0) { //envio no fim das 24h
    Serial.printf("Mes = %d Dia = %d horas = %d \n ", mesAux, diaAux, horasTotais);
    publish(0, mesAux, diaAux, horasTotais);
    resetVarAux();
  }
  
  int lead2 = digitalRead(LedBoard);
  //incrementa quantidade de horas com led ligado
  if (lead2 == LOW) { //lampada ligada
    if (horasAux != horarioCorreto && horasAux != -1 ) {
      horasTotais = horasTotais + 1;
    }

    diaAux = diaCorreto;
    mesAux = mesCorreto;
    horasAux = horarioCorreto;
  }
  
  //logica do temporizador
  if ((((diaTemp + 1) == diaCorreto) || (diaTemp == diaCorreto)) && temporizador == 1) {
    int lead = !digitalRead(LedBoard);
    if ( diaTemp == diaCorreto && horarioCorreto == horarioTemp && lead == LOW ) {
      digitalWrite(LedBoard, lead);
      publish(1, 0, 0, 0);
      Serial.println("Led ligou devido ao temporizador");
    }
    int horarioLimite = (horarioTemp + quantidadeHorasTemp) % 24;
    Serial.printf(" \nhorario limite = %d\n", horarioLimite);
    if (horarioCorreto == horarioLimite && lead == HIGH) {
      digitalWrite(LedBoard, lead);
      resetTemporizador();
      publish(2, 0, 0, 0);
      Serial.println("Led apagou devido ao temporizador");
    }
  }

  //publica o novo estado do Led, caso seja alterado pelo botão físico
  if (digitalRead(BUTTON) == LOW) {
    int lead = !digitalRead(LedBoard);
    if (lead == LOW) {
      publish(1, 0, 0, 0);
      Serial.println("Led ligou");
    }
    if (lead == HIGH) {
      Serial.printf("Mes = %d Dia = %d horas = %d \n ", mesAux, diaAux, horasTotais);
      publish(2, 0, 0, 0);
      Serial.println("Led apagou");
    }
    digitalWrite(LedBoard, lead);
    delay(300);
    Serial.println("Botão Pressionado");
  }
  client.loop();
}
