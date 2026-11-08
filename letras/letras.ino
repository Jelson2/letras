#include <WiFiManager.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "secrets.h"


#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define SCREEN_ADDRESS 0x3C
#define OLED_RESET -1

Adafruit_SSD1306 tela(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);



// SPOTIFY


// VARIÁVEIS SPOTIFY

String accessToken = "";

String idMusicaAtual = "";

String nomeMusicaAtual = "";
String artistaAtual = "";
String albumAtual = "";

unsigned long duracaoMusicaMs = 0;
unsigned long progressoMusicaMs = 0;

unsigned long ultimaVerificacao = 0;
unsigned long ultimoAjusteTempo = 0;

const unsigned long intervaloSpotify = 3000;

bool musicaTocando = false;

// LETRA

struct LinhaLetra {
  unsigned long tempoMs;
  String texto;
};

#define MAX_LINHAS 180

LinhaLetra letra[MAX_LINHAS];

int totalLinhas = 0;

int linhaAtualExibida = -1;


// SETUP


void setup() {

  Serial.begin(115200);

  // OLED


  if (!tela.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {

    Serial.println("Falha ao iniciar OLED");

    while (true) {
      delay(1000);
    }
  }

  tela.clearDisplay();
  tela.setTextColor(SSD1306_WHITE);
  tela.setTextSize(1);
  tela.setTextWrap(true);
  tela.setCursor(0, 0);

  tela.println("Conectando WiFi...");
  tela.display();

  // WiFiManager

  WiFiManager wm;

  wm.setConfigPortalBlocking(false);

  if (!wm.autoConnect("MeuESP", "MinhaSenha")) {

    Serial.println("Falha ao conectar WiFi");

    while (true) {

      wm.process();

      delay(10);
    }
  }


  // WiFi conectado

  Serial.println();
  Serial.println("WiFi conectado!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());


  tela.clearDisplay();

  tela.setCursor(0, 0);

  tela.println("WiFi conectado!");
  tela.println();
  tela.println("Sincronizando hora...");

  tela.display();

  // NTP


  configTime(
    0,
    0,
    "pool.ntp.org",
    "time.nist.gov"
  );

  time_t now = time(nullptr);

  while (now < 8 * 3600 * 2) {

    delay(500);

    now = time(nullptr);
  }
  // Spotify

  tela.clearDisplay();

  tela.setCursor(0, 0);

  tela.println("Spotify");
  tela.println("Autenticando...");

  tela.display();


  renovarAccessToken();


  if (accessToken != "") {

    tela.clearDisplay();

    tela.setCursor(0, 0);

    tela.println("Spotify pronto!");
    tela.println();
    tela.println("Aguardando musica...");

    tela.display();

  } else {

    tela.clearDisplay();

    tela.setCursor(0, 0);

    tela.println("Erro Spotify");
    tela.println("Token invalido");

    tela.display();
  }

  delay(1500);
}



void loop() {



  if (millis() - ultimaVerificacao >= intervaloSpotify) {

    ultimaVerificacao = millis();

    verificarSpotify();
  }

  // Estima progresso entre consultas


  if (musicaTocando) {

    unsigned long agora = millis();

    unsigned long tempoPassado =
      agora - ultimoAjusteTempo;

    progressoMusicaMs += tempoPassado;

    ultimoAjusteTempo = agora;


    // Evita ultrapassar a duração
    if (duracaoMusicaMs > 0 &&
        progressoMusicaMs > duracaoMusicaMs) {

      progressoMusicaMs = duracaoMusicaMs;
    }


    // Verifica se mudou a linha
    verificarSincroniaLetra();
  }
}




void renovarAccessToken() {

  Serial.println();
  Serial.println("==============================");
  Serial.println("Renovando Access Token...");
  Serial.println("==============================");


  WiFiClientSecure client;

  client.setInsecure();

  HTTPClient http;


  // ENDPOINT CORRETO DO SPOTIFY

  if (!http.begin(
        client,
        "https://accounts.spotify.com/api/token"
      )) {

    Serial.println("Falha ao iniciar HTTPS");

    return;
  }


  http.addHeader(
    "Content-Type",
    "application/x-www-form-urlencoded"
  );


  String corpoPost =
    "grant_type=refresh_token"
    "&refresh_token=" + String(SPOTIFY_REFRESH_TOKEN) +
    "&client_id=" + String(SPOTIFY_CLIENT_ID) +
    "&client_secret=" + String(SPOTIFY_CLIENT_SECRET);


  int httpCode = http.POST(corpoPost);


  Serial.print("Codigo HTTP token: ");
  Serial.println(httpCode);


  if (httpCode == 200) {

    String payload = http.getString();

    DynamicJsonDocument doc(2048);

    DeserializationError erro =
      deserializeJson(doc, payload);


    if (!erro) {

      accessToken =
        doc["access_token"].as<String>();


      if (accessToken != "") {

        Serial.println(
          "Access Token renovado com sucesso!"
        );

      } else {

        Serial.println(
          "Access Token vazio!"
        );
      }

    } else {

      Serial.println(
        "Erro ao interpretar JSON do token."
      );
    }

  } else {

    Serial.println(
      "Erro ao renovar Access Token."
    );

    Serial.println(
      http.getString()
    );
  }


  http.end();
}

// CONSULTAR SPOTIFY

void verificarSpotify() {

  if (accessToken == "") {

    return;
  }


  WiFiClientSecure client;

  client.setInsecure();

  HTTPClient http;


  // ENDPOINT CORRETO DA API DO SPOTIFY

  if (!http.begin(
        client,
        "https://api.spotify.com/v1/me/player"
      )) {

    Serial.println(
      "Falha ao iniciar conexao Spotify."
    );

    return;
  }


  http.addHeader(
    "Authorization",
    "Bearer " + accessToken
  );


  int httpCode = http.GET();


  Serial.print("Status Spotify: ");
  Serial.println(httpCode);

  // MÚSICA TOCANDO

  if (httpCode == 200) {

    String payload = http.getString();


    DynamicJsonDocument doc(8192);

    DeserializationError erro =
      deserializeJson(doc, payload);


    if (erro) {

      Serial.println(
        "Erro ao interpretar resposta Spotify."
      );

      http.end();

      return;
    }

    // Estado atual


    bool novoEstado =
      doc["is_playing"] | false;


    unsigned long novoProgresso =
      doc["progress_ms"] | 0;


    unsigned long novaDuracao =
      doc["item"]["duration_ms"] | 0;


    String idNova =
      doc["item"]["id"] | "";


    String novoNome =
      doc["item"]["name"] | "Desconhecido";


    String novoArtista =
      doc["item"]["artists"][0]["name"]
      | "Desconhecido";


    String novoAlbum =
      doc["item"]["album"]["name"]
      | "";



    // Verifica se mudou de música
 

    bool mudouMusica =
      (idNova != idMusicaAtual && idNova != "");

    // Atualiza informações

    musicaTocando = novoEstado;

    progressoMusicaMs = novoProgresso;

    duracaoMusicaMs = novaDuracao;

    ultimoAjusteTempo = millis();


    nomeMusicaAtual = novoNome;

    artistaAtual = novoArtista;

    albumAtual = novoAlbum;

    // NOVA MÚSICA


    if (mudouMusica) {

      idMusicaAtual = idNova;

      Serial.println();
      Serial.println("==============================");
      Serial.println("NOVA MUSICA");
      Serial.println("==============================");

      Serial.print("Musica: ");
      Serial.println(nomeMusicaAtual);

      Serial.print("Artista: ");
      Serial.println(artistaAtual);

      Serial.print("Album: ");
      Serial.println(albumAtual);

      Serial.print("Duracao: ");
      Serial.println(duracaoMusicaMs);


      // Limpa letra antiga

      totalLinhas = 0;

      linhaAtualExibida = -1;


      // Busca nova letra

      buscarLetraLRCLIB(
        nomeMusicaAtual,
        artistaAtual,
        albumAtual,
        duracaoMusicaMs
      );
    }

    // Música pausada

    if (!musicaTocando) {

      mostrarMusicaPausada();
    }


    // Música tocando

    else {

      verificarSincroniaLetra();
    }
  }

  // NENHUMA MÚSICA

  else if (httpCode == 204) {

    musicaTocando = false;

    mostrarNenhumaMusica();
  }



  // TOKEN EXPIRADO


  else if (httpCode == 401) {

    Serial.println(
      "Token expirado. Renovando..."
    );


    http.end();

    renovarAccessToken();

    return;
  }

  // PREMIUM / PERMISSÃO

  else if (httpCode == 403) {

    Serial.println(
      "Spotify retornou 403."
    );

    Serial.println(
      http.getString()
    );


    tela.clearDisplay();

    tela.setTextSize(1);

    tela.setCursor(0, 0);

    tela.println("Spotify bloqueou");
    tela.println();
    tela.println("Verifique Premium");
    tela.println("e autorizacao.");

    tela.display();
  }


  // OUTRO ERRO


  else {

    Serial.print(
      "Erro Spotify: "
    );

    Serial.println(httpCode);

    String resposta =
      http.getString();

    if (resposta.length() > 0) {

      Serial.println(resposta);
    }
  }


  http.end();
}

// BUSCAR LETRA NO LRCLIB


void buscarLetraLRCLIB(
  String nomeTrack,
  String nomeArtist,
  String nomeAlbum,
  unsigned long duracaoMs
) {

  totalLinhas = 0;

  linhaAtualExibida = -1;

  // Tela

  tela.clearDisplay();

  tela.setTextColor(SSD1306_WHITE);

  tela.setTextSize(1);

  tela.setCursor(0, 0);

  tela.println("Buscando letra...");

  tela.println();

  tela.println(nomeTrack);

  tela.println();

  tela.println(nomeArtist);

  tela.display();


  // URL LRCLIB


  String url =
    "https://lrclib.net/api/get";


  url += "?track_name=";

  url += urlEncode(nomeTrack);


  url += "&artist_name=";

  url += urlEncode(nomeArtist);


  if (nomeAlbum.length() > 0) {

    url += "&album_name=";

    url += urlEncode(nomeAlbum);
  }


  if (duracaoMs > 0) {

    unsigned long duracaoSeg =
      duracaoMs / 1000;

    url += "&duration=";

    url += String(duracaoSeg);
  }


  Serial.println();
  Serial.println("Consultando LRCLIB:");

  Serial.println(url);


  // HTTPS


  WiFiClientSecure client;

  client.setInsecure();

  HTTPClient http;


  if (!http.begin(client, url)) {

    Serial.println(
      "Falha ao iniciar LRCLIB."
    );

    mostrarLetraNaoEncontrada();

    return;
  }


  // Identificação do aplicativo

  http.addHeader(
    "User-Agent",
    "ESP32-Spotify-Lyrics/1.0"
  );


  int httpCode = http.GET();


  Serial.print(
    "Status LRCLIB: "
  );

  Serial.println(httpCode);


  if (httpCode == 200) {

    String payload =
      http.getString();


    Serial.println(
      "Resposta LRCLIB recebida."
    );


    DynamicJsonDocument doc(30000);


    DeserializationError erro =
      deserializeJson(doc, payload);


    if (erro) {

      Serial.println(
        "Erro ao interpretar JSON LRCLIB."
      );

      http.end();

      mostrarLetraNaoEncontrada();

      return;
    }


    // O LRCLIB retorna um objeto

    String letraSincronizada =
      doc["syncedLyrics"] | "";


    // Verifica se existe letra sincronizada
  

    if (
      letraSincronizada.length() > 0
    ) {

      Serial.println(
        "Letra sincronizada encontrada!"
      );


      parseLetraSincronizada(
        letraSincronizada
      );


      Serial.print(
        "Linhas carregadas: "
      );

      Serial.println(totalLinhas);


      if (totalLinhas > 0) {

        tela.clearDisplay();

        tela.setTextSize(1);

        tela.setCursor(0, 0);

        tela.println("Letra encontrada!");

        tela.println();

        tela.println(
          "Sincronizacao OK"
        );

        tela.display();

        delay(700);


        http.end();

        return;
      }
    }


    // Não encontrou sincronizada

    Serial.println(
      "Nao existe letra sincronizada."
    );
  }



  else if (httpCode == 404) {

    Serial.println(
      "Musica nao encontrada no LRCLIB."
    );
  }



  else {

    Serial.print(
      "Erro LRCLIB: "
    );

    Serial.println(httpCode);
  }


  http.end();

  mostrarLetraNaoEncontrada();
}


// URL ENCODE

String urlEncode(String texto) {

  String resultado = "";


  const char *hex =
    "0123456789ABCDEF";


  for (unsigned int i = 0;
       i < texto.length();
       i++) {

    char c = texto.charAt(i);


    if (
      (c >= 'a' && c <= 'z') ||
      (c >= 'A' && c <= 'Z') ||
      (c >= '0' && c <= '9') ||
      c == '-' ||
      c == '_' ||
      c == '.' ||
      c == '~'
    ) {

      resultado += c;

    } else {

      resultado += '%';

      resultado +=
        hex[(c >> 4) & 0x0F];

      resultado +=
        hex[c & 0x0F];
    }
  }


  return resultado;
}

// PARSER DA LETRA LRC

void parseLetraSincronizada(String dados) {

  totalLinhas = 0;


  int inicio = 0;


  while (
    inicio < dados.length() &&
    totalLinhas < MAX_LINHAS
  ) {

    int fim =
      dados.indexOf('\n', inicio);

    if (fim == -1) {

      fim = dados.length();
    }


    String linha =
      dados.substring(
        inicio,
        fim
      );


    linha.trim();




    if (linha.length() > 0) {

      int pos = 0;



      // Permite múltiplas tags na mesma linha
      // Exemplo:
      // [00:10.00][00:20.00]Texto


      while (
        pos < linha.length() &&
        linha.charAt(pos) == '['
      ) {

        int fecha =
          linha.indexOf(']', pos);


        if (fecha == -1) {

          break;
        }


        String tempoStr =
          linha.substring(
            pos + 1,
            fecha
          );


        unsigned long tempo =
          converterTempoLRC(
            tempoStr
          );

        if (
          tempoStr.indexOf(':') >= 0
        ) {

          String texto =
            linha.substring(
              fecha + 1
            );


          texto.trim();


          letra[totalLinhas].tempoMs =
            tempo;


          letra[totalLinhas].texto =
            texto;


          totalLinhas++;
        }


        pos = fecha + 1;
      }
    }


    inicio = fim + 1;
  }


  for (int i = 0;
       i < totalLinhas - 1;
       i++) {

    for (int j = i + 1;
         j < totalLinhas;
         j++) {

      if (
        letra[j].tempoMs <
        letra[i].tempoMs
      ) {

        LinhaLetra temporaria =
          letra[i];

        letra[i] =
          letra[j];

        letra[j] =
          temporaria;
      }
    }
  }
}

// CONVERTER TEMPO LRC

unsigned long converterTempoLRC(
  String tempo
) {

  int doisPontos =
    tempo.indexOf(':');


  if (doisPontos == -1) {

    return 0;
  }


  int minutos =
    tempo.substring(
      0,
      doisPontos
    ).toInt();


  String segundosStr =
    tempo.substring(
      doisPontos + 1
    );


  float segundos =
    segundosStr.toFloat();


  unsigned long resultado =
    (minutos * 60UL * 1000UL) +
    (unsigned long)(
      segundos * 1000.0
    );


  return resultado;
}


// SINCRONIZAÇÃO DA LETRA


void verificarSincroniaLetra() {

  if (totalLinhas == 0) {

    return;
  }


  int linhaAlvo = -1;


  // ---------------------------------------------------
  // Descobre a linha correspondente ao tempo
  // ---------------------------------------------------

  for (
    int i = 0;
    i < totalLinhas;
    i++
  ) {

    if (
      progressoMusicaMs >=
      letra[i].tempoMs
    ) {

      linhaAlvo = i;

    } else {

      break;
    }
  }


  // ---------------------------------------------------
  // Ainda não chegou na primeira linha

  if (linhaAlvo == -1) {

    return;
  }

  // Só atualiza OLED quando mudar a linha

  if (
    linhaAlvo != linhaAtualExibida
  ) {

    linhaAtualExibida =
      linhaAlvo;


    mostrarLinhaLetra(
      linhaAtualExibida
    );
  }
}



// MOSTRAR LETRA NO OLED

void mostrarLinhaLetra(int indice) {

  if (indice < 0 || indice >= totalLinhas) {
    return;
  }

  tela.clearDisplay();

  tela.setTextColor(SSD1306_WHITE);

  // Letra principal
  String texto = letra[indice].texto;

  desenharTextoCentralizado(
    texto,
    2,
    10
  );

  tela.display();

  Serial.print("[");
  Serial.print(formatarTempo(letra[indice].tempoMs));
  Serial.print("] ");
  Serial.println(letra[indice].texto);
}
String removerAcentos(String texto) {
  texto.replace("á", "a");
  texto.replace("à", "a");
  texto.replace("ã", "a");
  texto.replace("â", "a");
  texto.replace("ä", "a");
  texto.replace("Á", "A");
  texto.replace("À", "A");
  texto.replace("Ã", "A");
  texto.replace("Â", "A");
  texto.replace("Ä", "A");

  texto.replace("é", "e");
  texto.replace("è", "e");
  texto.replace("ê", "e");
  texto.replace("ë", "e");
  texto.replace("É", "E");
  texto.replace("È", "E");
  texto.replace("Ê", "E");
  texto.replace("Ë", "E");

  texto.replace("í", "i");
  texto.replace("ì", "i");
  texto.replace("î", "i");
  texto.replace("ï", "i");
  texto.replace("Í", "I");
  texto.replace("Ì", "I");
  texto.replace("Î", "I");
  texto.replace("Ï", "I");

  texto.replace("ó", "o");
  texto.replace("ò", "o");
  texto.replace("õ", "o");
  texto.replace("ô", "o");
  texto.replace("ö", "o");
  texto.replace("Ó", "O");
  texto.replace("Ò", "O");
  texto.replace("Õ", "O");
  texto.replace("Ô", "O");
  texto.replace("Ö", "O");

  texto.replace("ú", "u");
  texto.replace("ù", "u");
  texto.replace("û", "u");
  texto.replace("ü", "u");
  texto.replace("Ú", "U");
  texto.replace("Ù", "U");
  texto.replace("Û", "U");
  texto.replace("Ü", "U");

  texto.replace("ç", "c");
  texto.replace("Ç", "C");

  return texto;
}

void desenharTextoCentralizado(String texto, int tamanho, int y) {
  texto = removerAcentos(texto);
  tela.setTextSize(tamanho);
  tela.setTextWrap(false);

  int caracteresPorLinha = (tamanho == 2) ? 10 : 21;
  String palavras = texto;
  String linhas[4];
  int totalLinhasTexto = 0;

  while (palavras.length() > 0 && totalLinhasTexto < 4) {
    String linha = "";

    while (palavras.length() > 0) {
      int espaco = palavras.indexOf(' ');
      String palavra;

      if (espaco == -1) {
        palavra = palavras;
        palavras = "";
      } else {
        palavra = palavras.substring(0, espaco);
        palavras = palavras.substring(espaco + 1);
      }

      if (linha.length() == 0) {
        linha = palavra;
      } else if (linha.length() + palavra.length() + 1 <= caracteresPorLinha) {
        linha += " ";
        linha += palavra;
      } else {
        palavras = palavra + " " + palavras;
        break;
      }
    }

    linhas[totalLinhasTexto++] = linha;
  }

  int alturaLinha = 8 * tamanho;
  int alturaTotal = totalLinhasTexto * alturaLinha;
  int inicioY = (64 - alturaTotal) / 2;

  for (int i = 0; i < totalLinhasTexto; i++) {
    int largura = linhas[i].length() * 6 * tamanho;
    int x = (128 - largura) / 2;

    if (x < 0) x = 0;

    tela.setCursor(x, inicioY + (i * alturaLinha));
    tela.println(linhas[i]);
  }
}


// LIMITAR TEXTO

String limitarTexto(
  String texto,
  int quantidade
) {

  if (
    texto.length() <=
    quantidade
  ) {

    return texto;
  }


  return texto.substring(
    0,
    quantidade - 3
  ) + "...";
}
// MOSTRAR MÚSICA PAUSADA

void mostrarMusicaPausada() {

  tela.clearDisplay();

  tela.setTextColor(
    SSD1306_WHITE
  );

  tela.setTextSize(1);

  tela.setCursor(0, 0);

  tela.println("PAUSADO");

  tela.println();

  tela.println(
    nomeMusicaAtual
  );

  tela.println();

  tela.println(
    artistaAtual
  );

  tela.display();
}



void mostrarNenhumaMusica() {

  // Evita redesenhar desnecessariamente

  tela.clearDisplay();

  tela.setTextSize(1);

  tela.setCursor(0, 0);

  tela.println(
    "Nenhuma musica"
  );

  tela.println();

  tela.println(
    "tocando no momento."
  );

  tela.display();
}



void mostrarLetraNaoEncontrada() {

  totalLinhas = 0;

  linhaAtualExibida = -1;


  tela.clearDisplay();

  tela.setTextSize(1);

  tela.setCursor(0, 0);

  tela.println(
    "Letra sincronizada"
  );

  tela.println(
    "nao encontrada."
  );

  tela.println();

  tela.println(
    nomeMusicaAtual
  );

  tela.println();

  tela.println(
    artistaAtual
  );

  tela.display();
}



String formatarTempo(
  unsigned long ms
) {

  unsigned long segundos =
    ms / 1000;


  unsigned int minutos =
    segundos / 60;


  segundos =
    segundos % 60;


  char buffer[10];


  sprintf(
    buffer,
    "%02u:%02lu",
    minutos,
    segundos
  );


  return String(buffer);
}
