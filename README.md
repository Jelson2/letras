# 🎵 ESP32 Spotify Lyrics

Um display de letras sincronizadas para o que você está ouvindo no Spotify, feito com **ESP32** e um **OLED SSD1306**. O ESP32 consulta a música que está tocando, busca a letra sincronizada no [LRCLIB](https://lrclib.net) e mostra no display a linha certa no momento certo, como um karaokê em miniatura.

<!-- Adicione aqui uma foto ou GIF do projeto funcionando -->
<!-- ![Demo](docs/demo.gif) -->

## ✨ Funcionalidades

- Detecta em tempo real a música tocando no Spotify (qualquer dispositivo da sua conta)
- Busca automática da letra sincronizada (formato LRC) no LRCLIB
- Letra centralizada no OLED, com quebra automática de linha
- Progresso da música estimado localmente entre as consultas, para a letra não "travar"
- Configuração de Wi-Fi sem mexer no código, via portal do **WiFiManager**
- Renovação automática do token de acesso do Spotify
- Telas de estado: pausado, nenhuma música tocando, letra não encontrada, erros de autenticação
- Remoção de acentos para exibir bem na fonte padrão do display

## 🧰 Hardware necessário

| Item | Observação |
|------|------------|
| ESP32 (DevKit ou similar) | Com Wi-Fi |
| Display OLED SSD1306 128x64 | Interface I2C, endereço `0x3C` |
| Jumpers / protoboard | |

### Ligações

| OLED | ESP32 |
|------|-------|
| VCC  | 3V3 |
| GND  | GND |
| SDA  | GPIO 21* |
| SCL  | GPIO 22* |

\* Pinos I2C padrão da maioria das placas ESP32 DevKit. Confirme na sua placa ou altere com `Wire.begin(SDA, SCL)`.

## 📚 Bibliotecas

Instale pela Arduino IDE (Gerenciador de Bibliotecas) ou PlatformIO:

- [WiFiManager](https://github.com/tzapu/WiFiManager)
- [ArduinoJson](https://arduinojson.org/) **v6** (o código usa `DynamicJsonDocument`)
- [Adafruit GFX Library](https://github.com/adafruit/Adafruit-GFX-Library)
- [Adafruit SSD1306](https://github.com/adafruit/Adafruit_SSD1306)

`WiFiClientSecure`, `HTTPClient`, `SPI` e `Wire` já vêm com o core do ESP32.

## 🔑 Configurando o Spotify

O projeto usa o fluxo de **refresh token**, então você precisa gerar as credenciais uma vez.

1. Acesse o [Spotify Developer Dashboard](https://developer.spotify.com/dashboard) e crie um app.
2. Anote o **Client ID** e o **Client Secret**.
3. Adicione uma Redirect URI ao app (por exemplo `http://127.0.0.1:8888/callback`).
4. Faça o fluxo de autorização (Authorization Code) pedindo o escopo `user-read-playback-state` e troque o código recebido por um **refresh token**.
5. Guarde os três valores para o próximo passo.

> ⚠️ A API de estado do player (`/v1/me/player`) pode exigir conta **Spotify Premium** e as permissões corretas. Se o display mostrar "Spotify bloqueou", verifique isso.

### Arquivo `secrets.h`

Dentro da pasta `letras/`, copie o `secrets.example.h` para `secrets.h` e preencha com suas credenciais:

```cpp
#pragma once

#define SPOTIFY_CLIENT_ID      "seu_client_id"
#define SPOTIFY_CLIENT_SECRET  "seu_client_secret"
#define SPOTIFY_REFRESH_TOKEN  "seu_refresh_token"
```

> 🔒 **Nunca envie o `secrets.h` para o GitHub.** Adicione ao `.gitignore`:
>
> ```
> secrets.h
> ```
>
> Se quiser, inclua um `secrets.example.h` com valores falsos para quem for clonar o projeto.

## 🚀 Como usar

1. Clone o repositório:
   ```bash
   git clone https://github.com/Jelson2/letras.git
   ```
2. Crie o `secrets.h` (veja acima).
3. 3. Abra o arquivo `letras/letras.ino` na Arduino IDE, selecione sua placa ESP32 e a porta, e faça o upload.
4. Na primeira execução, conecte-se à rede Wi-Fi **`MeuESP`** (senha: `MinhaSenha`) e escolha sua rede no portal que abrir.
5. Toque uma música no Spotify e acompanhe a letra no display.

Abra o Monitor Serial a **115200 baud** para ver logs de depuração (música atual, status das requisições, linhas da letra).

## ⚙️ Como funciona

```
ESP32 ──► Spotify API  (a cada 3 s: música, progresso e estado)
  │
  ├──► LRCLIB API      (ao trocar de música: letra sincronizada .lrc)
  │
  └──► OLED            (mostra a linha correspondente ao progresso atual)
```

1. Ao ligar, o ESP32 conecta ao Wi-Fi, sincroniza a hora via NTP e renova o access token do Spotify.
2. A cada **3 segundos** consulta `/v1/me/player` para saber música, duração, progresso e se está tocando.
3. Quando a música muda, busca a letra no LRCLIB usando nome, artista, álbum e duração.
4. O arquivo `.lrc` é interpretado em uma lista de linhas com tempo em milissegundos (até **180 linhas**).
5. Entre uma consulta e outra, o progresso é estimado com `millis()`, e o display só é redesenhado quando a linha muda.
6. Se a API devolver `401`, o token é renovado automaticamente.

## 🛠️ Personalização

| O que mudar | Onde |
|-------------|------|
| Intervalo de consulta ao Spotify | `intervaloSpotify` (padrão: 3000 ms) |
| Máximo de linhas por letra | `MAX_LINHAS` (padrão: 180) |
| Endereço I2C do OLED | `SCREEN_ADDRESS` (padrão: `0x3C`) |
| Nome/senha do portal Wi-Fi | `wm.autoConnect("MeuESP", "MinhaSenha")` |
| Tamanho da fonte da letra | segundo parâmetro de `desenharTextoCentralizado()` |

## 🐞 Problemas comuns

- **"Falha ao iniciar OLED"**: confira fios SDA/SCL e o endereço I2C (alguns módulos usam `0x3D`).
- **"Erro Spotify / Token invalido"**: revise Client ID, Client Secret e refresh token no `secrets.h`.
- **"Spotify bloqueou"**: erro 403. Verifique Premium e o escopo `user-read-playback-state`.
- **"Letra sincronizada nao encontrada"**: a música pode não ter letra sincronizada no LRCLIB.
- **Letra adiantada ou atrasada**: ajuste o intervalo de consulta ou aplique um deslocamento (offset) no tempo da letra.

## 🗺️ Ideias para o futuro

- [ ] Barra de progresso e nome da música no display
- [ ] Suporte a caracteres acentuados com fonte customizada
- [ ] Botão físico para pausar/pular faixa
- [ ] Fallback para letra não sincronizada
- [ ] Verificação de certificado TLS (hoje usa `setInsecure()`)

## 🙏 Créditos

- [LRCLIB](https://lrclib.net): API gratuita de letras sincronizadas
- [Spotify Web API](https://developer.spotify.com/documentation/web-api)
- Bibliotecas Adafruit, WiFiManager e ArduinoJson

## 📄 Licença

Distribuído sob a licença MIT. Veja o arquivo `LICENSE` para mais detalhes.

---

Feito por [Jelson](https://github.com/Jelson2) 💚
