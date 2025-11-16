[English](README.md) | [Português (BR)](README.pt-BR.md) 

# 🎮 OBS GameDetector  
Plugin para detectar jogos instalados e integrar com Twitch · Suporte para OBS Studio

---

## 📘 Sobre o OBS GameDetector

OBS GameDetector é um plugin para OBS Studio que identifica automaticamente jogos instalados no seu PC (Steam e Epic Games), permitindo:

- Seleção automática de jogo
- Integração com Twitch (Client ID + Access Token)
- Correção e edição de nomes e executáveis detectados
- Criação automática de metadados
- Interface amigável dentro do OBS

O foco é velocidade, detecção precisa e zero impacto no desempenho.

---

## 📥 Instalação

Após baixar o instalador ou o arquivo ZIP:

### **Instalação pelo instalador (recomendado)**
1. Baixe o arquivo **OBSGameDetector-Setup.exe** da página de [Releases](releases).
2. Execute o instalador.
3. Abra o OBS e confirme que o plugin aparece no menu **Ferramentas → OBS GameDetector**.

### **Instalação manual pelo ZIP**
1. Extraia o ZIP.
2. Copie:
   - `obs-plugins/64bit/obs-game-detector.dll` → para a pasta de plugins do OBS  
   - `data/obs-plugins/obs-game-detector/` → para a pasta de dados do OBS  
3. Reinicie o OBS.

---

## 🔧 Configuração do Twitch

O plugin possui dois campos obrigatórios para integração com a Twitch:

- **Client ID**
- **Access Token**

### Como preencher:

1. Abra o OBS.
2. Vá em **Ferramentas → OBS GameDetector**.
3. No painel de configurações, clique no botão **Gerar Token**.
4. Você será enviado para:

   👉 https://twitchtokengenerator.com

5. No site, gere o token normalmente.
6. Copie **exatamente estes dois campos**:
   - **ACCESS TOKEN**
   - **CLIENT ID**
7. Cole-os nos campos dentro do plugin:
   - **Client ID**
   - **Access Token**
8. Clique em **Salvar**.

⚠️ Nenhuma senha da Twitch é solicitada ou utilizada.  
⚠️ Somente os dois campos acima são necessários.

---

## 🎮 Tabela de Jogos Detectados

Após a varredura, o plugin exibe uma tabela com todos os jogos encontrados.

A detecção é rápida pois o plugin **não varre o computador inteiro**, apenas:

- ✔️ Pastas da Steam Library
- ✔️ Diretórios padrão da Epic Games

Isso evita lentidão, falsos positivos e leituras desnecessárias.

---

## ✏️ Edição dos Jogos Encontrados

A tabela permite editar:

### ✔️ Nome do jogo  
Quando o nome detectado não coincide com o nome desejado.

### ✔️ Nome do executável (.exe)  
Útil quando o jogo possui múltiplos executáveis ou quando o arquivo detectado não é o principal.

### ✔️ Caminho completo  
Somente para ajustes manuais, caso necessário.

As alterações são salvas automaticamente.

---

## 🔄 Re-scan de jogos

Você pode executar a busca novamente a qualquer momento:

📌 Clique no botão **Re-scan** dentro da janela do plugin.

---

## 🖼️ Screenshots (placeholders)

> Substitua as imagens abaixo com capturas reais.

### Tela principal:
![main-ui](./screenshots/main.png)

### Detecção de jogos:
![games-list](./screenshots/games.png)

### Configuração:
![settings](./screenshots/settings.png)

---

## 🧩 Compatibilidade

| Recurso                   | Suporte |
|---------------------------|---------|
| OBS Studio               | ✔️ 29+  |
| Windows                  | ✔️ 10/11 64-bit |
| Steam Games              | ✔️ |
| Epic Games               | ✔️ |
| Outros launchers         | ❌ (planejado para futuro) |

---

## 🛠️ Tecnologias utilizadas

- C++  
- libobs  
- Qt6  
- OBS Frontend API  
- Twitch API  
- Inno Setup  

---

## 🤝 Créditos

Desenvolvido por **Fábio F. Magalhães**.  
Contribuições e PRs são bem-vindos!

---

## 📄 Licença

Este projeto é distribuído sob a licença **MIT**.

---

## ⭐ Suporte o projeto

Se o plugin te ajudou, considere deixar uma estrela ⭐ no GitHub!