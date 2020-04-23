//=======================================================================================//
/*
 
 Radioenge Equip. de Telecom.
 Abril de 2020
 Código exemplo para uso dos módulos LoRaMESH com o ESP8266 + ThingSpeak
 
 Fontes consultadas:
https://circuits4you.com/2018/01/02/esp8266-timer-ticker-example/ 08-04-2020
https://stackoverflow.com/questions/5697047/convert-serial-read-into-a-useable-string-using-arduino //08/04/2020
https://www.filipeflop.com/blog/esp8266-com-thingspeak/ //13-04-2020

 */



#include <ESP8266WiFi.h>
#include <Ticker.h>
#include<SoftwareSerial.h> 
//========SoftwareSerial==================
SoftwareSerial SWSerial(4, 5);  //D2, D1 : Será usada a serial por software para deixar livre a serial do monitor serial
//===============Timer====================
Ticker blinker;

// =====================WiFi======================
#define WIFI_SSID "Radioenge_0x06" //ssid da rede wifi / rede sem fio 
#define WIFI_PASSWORD "rad1oenge." //senha da rede sem fio
//===============================================

//constantes e variáveis globais
char EnderecoAPIThingSpeak[] = "api.thingspeak.com";
String ChaveEscritaThingSpeak = "6S9XXXXXXXXXX5T4"; //Write API Key da sua conta no ThingSpeak.com
long lastConnectionTime; 
WiFiClient client;
int count = 0;
char inData[255]; // Buffer com tamanho suficiente para receber qualquer mensagem
char inChar=-1; // char para receber um caractere lido
byte indice = 0; // índice para percorrer o vetor de char = Buffer
int listaEndDevices[5] = {1,2,1,2,1};//pode aumentar o tamanho do array e incluir um novo ID. coloquei 1,2,1,2,1 ... porque no teste só temos os IDs 1 e 2 ... em uma rede maior ... deve-se cololcar os IDs existentes 1,2,3,50,517, 714,... qualquer inteiro de 1 até 1023.
int polingID=0;//usado para escolher qual ID do vetor acima será usado.
char flag= false; //flag usada para avisar que a serial recebeu dados
char comandos = 0;//usado para escolher qual comando enviar.
char listaGPIO[]={0,5}; // usado para ler as GPIOs que estão neste vetor ... pode-se ler da 0 até 7
//===============================================================================
//Timer : Quando este timer "estourar" ... o led mudará seu estado e será incrementado o tempo usado no polling.
void ICACHE_RAM_ATTR onTimerISR(){
    digitalWrite(LED_BUILTIN,!(digitalRead(LED_BUILTIN)));  //Toggle LED Pin
    timer1_write(600000);//12us
    count++;
}
//================================================================================

// ===================== protótipos das funções  =================================
char lerSerial();
char TrataRX(byte indice);
void escreve(int contador);
char LerGPIO(int id, char gpio);
char SetGPIO(int id, char gpio, char nivel);
uint16_t CalculaCRC(char* data_in, uint32_t length);
//================================================================================


void setup() {

  Serial.begin(9600);
  SWSerial.begin(9600);
  // connect to wifi.
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("conectando");
  count=0;
  while ((WiFi.status() != WL_CONNECTED) || count<20 ) 
  {
    Serial.print("Conectando...\n");
    delay(500);
    count++;
  }
  Serial.println();
  Serial.print("conectado: ");
  Serial.println(WiFi.localIP());
  pinMode(LED_BUILTIN, OUTPUT); // Initialize the LED_BUILTIN pin as an output
  //Initialize Ticker every 0.5s
  timer1_attachInterrupt(onTimerISR);
  timer1_enable(TIM_DIV16, TIM_EDGE, TIM_SINGLE);
  timer1_write(600000); //120 ms altera o tempo do timer para 120ms
}




// the loop function runs over and over again forever
void loop() {
  if(count % 250 == 0)//30 : Se o contador que é incrementado no timer chegar a 30 segundos (120ms * 250=30) ... envia um comando para um escravo. o comando será RSSI, depois Leitura da porta analógica 5 e por últio
  {
        count++;
        if((comandos%3) ==0)// %3 porque são 3 comandos diferente para cada EndDevice / Radio. Podem ser incluídos novos comandos. o Primeiro é a medida de nível de sinal de recepção
        {
          LerNivelRx(listaEndDevices[polingID%5]);//%5 porque o array com IDs de EndDevices a ser percorrido só tem 5 endereços... pode-se diminuir ou aumentar ... depende do número de IDs/EndDevices existentes na rede
          
        }
        else if((comandos%3) == 1) // %3 porque são 3 comandos diferente para cada EndDevice / Radio. Podem ser incluídos novos comandos 
        {
          LerGPIO(listaEndDevices[polingID%5], listaGPIO[0]);//%5 porque o array com IDs de EndDevices a ser percorrido só tem 5 endereços
         
        }
        else if((comandos%3) == 2) //podem ser incluídos novos comandos 
        {
         LerGPIO(listaEndDevices[polingID%5], listaGPIO[1]);//%5 porque o array com IDs de EndDevices a ser percorrido só tem 5 endereços ... se mudar o array listaEndDevices[] também mude o valor desta divisão por módulo.
          polingID++;//Enviou os 3 comandos diferentes para um ID... então ... vai para o próximo ID/Rádio/EndDevice
        }
        comandos++;//próximo comando
  }
    flag= LerSerial(); // verifica se a serial por software recebeu alguma informação. Ser quiser usar a serial nativa para receber dados...  tem que implementar uma nova função.
    if (flag)//pode-se colocar o LerSerial() dentro do if, mas fica mais claro de mostrar usando a flag.
    {
      TrataRX(indice);//chama a função que vai interpretar o que foi recebido na serial por software
    }

}

//=========================== função para vericar se recebeu alguma informação na serial por software pinos D1 e D2 ===================
char LerSerial( ) 
{
    indice=0;
    while (SWSerial.available() > 0) // Don't read unless// there you know there is data
    {
        if(indice < 254) // One less than the size of the array
        {
            inChar = SWSerial.read(); // Read a character
            inData[indice] = inChar; // Store it
            delay (10);
            indice++; // Increment where to write next
            inData[indice] = '\0'; // Null terminate the string
        }        
    }
    if(indice>0) 
    {
        return(1); //retorno da função avisando que recebeu dados na serial
    }
    else
    {
      return (0);
    }

}
//-----------------------------------------------------------------------------//

//================== tratamento do que recebeu na serial ========================
char TrataRX(byte indice) //IMPORATANTE: O terceiro byte [2] é sempre o comando... sabendo o comando sabemos como tratar o restante do pacote
{
      Serial.println(inData); //mostra no console serial
      if(indice>5)//verifica se recebeu uma reposta de comando maior que 5 bytes para não ler dados inválidos
      {
        int id=inData[0]+inData[1]*256;//id msb*256+id lsb converte os dois primeiros bytes em um int que é o id do EndDevice de origem
        if(inData[2] == 0xD5)//0xD5 ou 213 decimal é o comando de leitura de nível de RX byte3=máximo byte 4 = médio byte 5=nmínimo
        {
          char sinal = inData[6]; //valor médio do nível de RX do EndDevice no sétimo byte do array
          char buf[35];//buffer para montar uma string a ser envida na serial do monitor
          sprintf(buf,"ID Origem: %d Nivel RX: -%d dBm",id, sinal);  //format two ints into character array
          Serial.println(buf);
        }
        if(inData[2] == 0xC2 )//resposta de comando na GPIO
        {
          if(inData[4] == 0x00 && inData[3] == 0x00)//[4]== 0: resposta ok. sem erro! [3] == 0: resposta de leitura
          {
            
            if(inData[6] & 0x80)//verifica se o primeiro bit do byte 6 é 1... se for 1.. o pacote contém a resposta de uma porta digitial então o próximo byte pode ser zero ou um
            {
              Serial.println("Leitura de GPIO / Porta Digital"); //imprime na serial do monitor ... somente para acompanhar o que está acontecendo.
              Serial.print("EndDevice id: ");//imprime na serial do monitor .
              Serial.print(id);//imprime na serial do monitor .
              Serial.print("  GPIO: ");//imprime na serial do monitor .
              Serial.println((int)inData[5]);//qual GPIO de 0 a 7//imprime na serial do monitor .
              char infoTTS[15] = {0};
              char valor = '0';
              if(inData[7] == 1)
              {             
                Serial.println("Nível Alto");//imprime na serial do monitor
                valor = '1';  
                Serial.println("Envia comando para ligar a gpio07");//imprime na serial do monitor
                SetGPIO(id, 0x07, 0x01); //IMPORTANTE: Aqui temos uma ação... Se a entrada digital estiver em nível alto então... é chamada a função SetGPIO que por sua vez envia o comando para ligar a o gpio 7 e acinar o Relé ...
              }
              else
              {
                Serial.println("Nível Baixo");
                valor = '0';
                Serial.println("Envia comando para desligar a gpio07");
                SetGPIO(id, 0x07, 0x00); //IMPORTANTE: Aqui temos uma ação... Se a entrada digital estiver em baixo alto então... é chamada a função SetGPIO que por sua vez envia o comando para desligar a o gpio 7 e acinar o Relé ...
              }
               if(id == 1)//Aqui como na rede de testes temos somente o ID 1 e ID2
                {
                  sprintf(infoTTS,"field2=%c", valor );
                }
                else if(id == 2)
                {
                  sprintf(infoTTS,"field4=%c", valor );
                }
              EnvioTheThingSpeak(infoTTS);
            }
            else //resposta de leitura de uma porta analógica
            {
              Serial.println("Leitura de GPIO / Porta Analógica");//imprime na serial do monitor 
              Serial.print("EndDevice id: ");
              Serial.print(id);
              Serial.print("  GPIO: ");
              Serial.print((int)inData[5]);//qual GPIO de 0 a 7
              int valorAD=inData[6]*256+inData[7];
              Serial.print("Leitura em BITs: ");
              Serial.println(valorAD);
              Serial.print("Leitura em Volts: ");
              Serial.println((3.3/4096)*valorAD);
              char infoTTS[15] = {0};
              if(id == 1)
                {
                  sprintf(infoTTS,"field1=%.2f", ((3.3/4096)*valorAD) );
                }
              else if(id == 2)
                {
                  sprintf(infoTTS,"field3=%.2f", ((3.3/4096)*valorAD) );
                }
              EnvioTheThingSpeak(infoTTS);
            }
          }
          else if(inData[4] == 0x01)
          {
            Serial.println("Erro na leitura ou tentativa de ler uma porta configurada como saída.");
          }
          else if(inData[3] == 0x01)
          {
            Serial.println("Comando de setGpio foi recebido pelo endDevice.");
          }
          
        }
      }
}

//====================================================
void escreve(int contador)
{
   Serial.println(count);
}
//=====================================================

//===================cálculo do CRC16==================
/**
*@brief Calcula CRC16.
*@param data_in: Ponteiro para o buffer contendo os dados.
*@param length: Tamanho do buffer
*@retval Valor de 16 bits representando o CRC16 do buffer
fornecido. */
#define CRC_POLY (0xA001)
uint16_t CalculaCRC(char* data_in, uint32_t length)
{
uint32_t i;
uint8_t bitbang, j;
uint16_t crc_calc;
crc_calc = 0xC181;
for(i=0; i<length; i++)
{
crc_calc ^= ((uint16_t)data_in[i]) & 0x00FF;
for(j=0; j<8; j++)
{
bitbang = crc_calc;
crc_calc >>= 1;
if(bitbang & 1)
{
crc_calc ^= CRC_POLY;
}
}
}
return crc_calc;
}
//==========================================================================
 
//===================  função para leitura no nível de RX  =================
char LerNivelRx(int id)
{
  char id_lsb = id&0xFF; //separando o ind em dois bytes para enviar no pacote; &0xFF pega somente os 8 bits menos significativos
  char id_msb = (id>>8)&0xFF;//bitwise desloca os 8 bits mais signficativos e pega somente a parte msb do int
  char comando = 0xD5;//comando de leitura do nível de recepção
  char pacote[]={id_lsb, id_msb, comando,0x00, 0x00, 0x00, 0x00,0x00};
  int crc = CalculaCRC(pacote, 6);
  pacote[6] = crc&0xFF;
  pacote[7]= ((crc>>8)&0xFF);
  SWSerial.write(pacote,8);//para enviar qualquer pacote para o EndDevice use a função write!
  Serial.print("Envio pacote leitura de nível de RX \"RSSI\" para o ID: ");//somente para aparecer no monitor serial do Arduino
  Serial.println(id);
  return(1);
}

//==========================================================================

//======================  função para leitura das GPIO  ====================
char LerGPIO(int id, char gpio)
{
  char id_lsb = id&0xFF; //separando o ind em dois bytes para enviar no pacote; &0xFF pega somente os 8 bits menos significativos
  char id_msb = (id>>8)&0xFF;//bitwise desloca os 8 bits mais signficativos e pega somente a parte msb do int
  char comando = 0xC2; //comando referente a GPIO
  char sub_cmd_leitura_gpio = 0x00; // subcomando de leitura da GPIO
  char pacote[]={id_lsb, id_msb, comando, sub_cmd_leitura_gpio, gpio, 0x00, 0x00,0x00};
  int crc = CalculaCRC(pacote, 6);
  pacote[6] = crc&0xFF;
  pacote[7]= ((crc>>8)&0xFF);
  SWSerial.write(pacote,8);
  Serial.print("Envio pacote leitura de GPIO para o ID: ");//somente para aparecer no monitor serial do Arduino
  Serial.print(id);
  Serial.print(" GPIO: ");
  Serial.println(gpio);
  return(1);
}

//==========================================================================

//======================  função para Acionamento da GPIO  ====================
char SetGPIO(int id, char gpio, char nivel) //02 00 C2 01 07 01 00 6A 65  //02 00 C2 01 07 00 00 6B F5 
{
  char id_lsb = id&0xFF; //separando o ind em dois bytes para enviar no pacote; &0xFF pega somente os 8 bits menos significativos
  char id_msb = (id>>8)&0xFF;//bitwise desloca os 8 bits mais signficativos e pega somente a parte msb do int
  char comando = 0xC2; // comando referente a GPIO
  char sub_cmd_escrita_gpio = 0x01; //comando de escrita/acionamento da GPIO
  char pacote[]={id_lsb, id_msb, comando, sub_cmd_escrita_gpio, gpio, nivel, 0x00,0x00};
  int crc = CalculaCRC(pacote, 6);
  pacote[6] = crc&0xFF;
  pacote[7]= ((crc>>8)&0xFF);
  SWSerial.write(pacote,8);
  Serial.print("Envio pacote escrita de GPIO para o ID: ");//somente para aparecer no monitor serial do Arduino
  Serial.print(id);
  Serial.print(" GPIO: ");
  Serial.print(gpio);
  Serial.print(" Nível: ");
  Serial.println(nivel);
  return(1);
}

//==========================================================================



char EnvioTheThingSpeak(String dados)
{
if (client.connect(EnderecoAPIThingSpeak, 80))
    {
        /* faz a requisição HTTP ao ThingSpeak */
        client.print("POST /update HTTP/1.1\n");
        client.print("Host: api.thingspeak.com\n");
        client.print("Connection: close\n");
        client.print("X-THINGSPEAKAPIKEY: "+ChaveEscritaThingSpeak+"\n");
        client.print("Content-Type: application/x-www-form-urlencoded\n");
        client.print("Content-Length: ");
        client.print(dados.length());
        client.print("\n\n");
        client.print(dados);
        Serial.println("- Informações enviadas ao ThingSpeak!");
        Serial.println(dados);
    }
}
