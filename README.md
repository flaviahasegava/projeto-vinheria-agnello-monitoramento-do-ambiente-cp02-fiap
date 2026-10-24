# 🍷 Projeto: O caso da Vinheria Agnello (CP02) - Monitoramento de Ambiente
- A Adega Agnello é um projeto desenvolvido para monitorar as condições ambientais de uma adega, acompanhando três fatores importantes: temperatura, umidade do ar e luminosidade.
- A ideia é utilizar um Arduino para coletar os dados dos sensores, mostrar as informações em uma tela LCD e emitir alertas quando alguma condição estiver fora dos limites definidos.
- Além do monitoramento, o sistema permite personalizar algumas configurações e guardar um histórico de situações críticas.
  
# 🚥 Como funciona?
O sistema realiza leituras dos sensores aproximadamente a cada segundo e calcula a média dos valores em intervalos de cinco segundos, ajudando a reduzir pequenas oscilações nas medições. Com base nos resultados, o Arduino identifica o estado do ambiente:
- **🟢 Verde:** condições dentro dos limites definidos.
- **🟡 Amarelo:** situação que precisa de atenção.
- **🔴 Vermelho:** condição crítica, acompanhada de um alerta sonoro.

As informações são exibidas em um LCD, que possui diferentes telas para mostrar o resumo, a temperatura, a umidade, a luminosidade e a data e hora.

# 🔹 Principais funcionalidades
- Monitoramento de temperatura, umidade e luminosidade.
- Cálculo de médias das medições a cada cinco segundos.
- Alertas visuais com LEDs e alertas sonoros com buzzer.
- Exibição de informações em um LCD 16x2.
- Relógio para registrar data e hora das ocorrências.
- Histórico de situações críticas salvo na EEPROM.
- Configuração de fuso horário, unidade de temperatura e idioma.
- Animação de uma garrafa na inicialização com a logo da nossa empresa.
- Consulta e limpeza dos registros pelo monitor serial.

# 🔘 Configurações pelo botão
Um dos diferenciais do projeto é a possibilidade de alterar as configurações sem precisar modificar o código. O botão possui duas funções:
- **Clique curto:** troca a tela ou altera uma opção no menu.
- **Clique longo:** abre o menu de configurações ou confirma uma escolha.

É possível escolher:
- **Fuso horário.**
- **Temperatura em Celsius ou Fahrenheit.**
- **Idioma em português, inglês ou espanhol.**

As configurações ficam salvas na EEPROM e são recuperadas quando o sistema é ligado novamente.

# 💾 Armazenamento dos registros
- A EEPROM é utilizada para guardar as configurações e o histórico de situações críticas.
- Cada registro contém a data e hora, a temperatura, a umidade e a luminosidade. O sistema pode armazenar até 126 registros em uma EEPROM de 1 KB, considerando a organização definida no código.
- Quando a memória destinada aos registros fica cheia, os mais antigos são substituídos pelos novos.

Para consultar o histórico, basta utilizar o monitor serial com velocidade de 115200 baud:
- **Digite l:** listar os registros.
- **Digite c**: limpar o histórico.

# 🛠️ Hardware e Componentes Utilizados
Para desenvolver o projeto Adega Agnello, foram utilizados os seguintes componentes:
- **1x Arduino Uno R3:** responsável por controlar o sistema e processar os dados dos sensores.
- **1x Sensor DHT22:** utilizado para medir a temperatura e a umidade do ar.
- **1x Sensor LDR:** responsável por detectar a luminosidade do ambiente.
- **1x Display LCD 16x2 com módulo I2C:** utilizado para exibir as medições, os alertas e as configurações.
- **1x Módulo RTC DS1307:** responsável por fornecer a data e a hora.
- **1x LED Verde:** indica que as condições estão dentro dos limites definidos.
- **1x LED Amarelo:** indica uma situação de atenção.
- **1x LED Vermelho:** indica uma condição crítica.
- **1x Buzzer:** emite alertas sonoros quando uma condição está crítica.
- **1x Botão de pressão:** permite trocar as telas e acessar as configurações do sistema.
- **Resistores (220Ω):** utilizados nas ligações elétricas, conforme a necessidade do circuito.
- **Jumpers (Macho-Macho e Macho-Fêmea):** utilizados para conectar os componentes.
- **1x Protoboard:** facilita a montagem do circuito.
- **1x Bateria compatível com o módulo RTC, se necessária:** mantém o relógio funcionando quando a alimentação principal é desligada.
- **1x Cabo USB compatível com a placa:** utilizado para programar e alimentar o Arduino.

**Observação:** o código está configurado para o DHT22 e para o módulo RTC DS1307. Antes de montar o circuito, confira os componentes disponíveis e os pinos definidos no programa.

# 📚 Bibliotecas Necessárias
- **Wire.h** - comunicação I2C.
- **LiquidCrystal_I2C.h** - controle do display LCD.
- **RTClib.h** - comunicação com o relógio RTC.
- **DHT.h** - leitura do sensor de temperatura e umidade.
- **EEPROM.h** - armazenamento das configurações e dos registros.

Para que o código funcione, é preciso instalar a biblioteca no Arduino IDE:
1. Abra o programa Arduino IDE.
2. Clique no canto superior esquerdo **Sketch** > **Include Library** > **Manage Libraries... or Ctrl + Shift + I**.
3. Pesquise as bibliotecas.
4. Clique em **Install**.

# 🔌 Pinos utilizados
- **Sensor DHT22** -> **D2** (Pino digital).
- **Botão** -> **D6** (Pino digital).
- **Buzzer** -> **D7** (Pino digital).
- **LED Verde** -> **D9** (Pino digital).
- **LED Amarelo** -> **D10** (Pino digital).
- **LED Vermelho** -> **D11** (Pino digital).
- **Sensor LDR (Luminosidade)** -> A1 (Entrada analógica).
- **Display LCD 16x2 com módulo I2C** -> **SDA** (**A4**) e **SCL** (**A5**)
O display LCD utiliza o endereço I2C 0x27 no código. Os pinos SDA e SCL dependem da placa Arduino utilizada.

# 💻 Como executar o projeto
1. Monte o circuito de acordo com os pinos definidos no código.
2. Clone este repositório ou faça o download dos arquivos em formato ZIP.
3. Abra o arquivo ".ino" no Arduino IDE.
4. Instale as bibliotecas necessárias pela Arduino IDE.
5. Abra o arquivo do projeto na Arduino IDE.
6. Confira se os modelos dos sensores e do RTC correspondem aos componentes utilizados.
7. Selecione a placa e a porta corretas.
8. Envie o código para o Arduino. Clique no botão **Upload** (ícone de seta para a direita) para compilar e carregar o código para o Arduino.

- Abra o monitor serial em 115200 baud para acompanhar as mensagens e consultar os registros.
- Na primeira inicialização, o sistema realiza a calibração inicial do sensor de luminosidade e começa o monitoramento do ambiente.

Para configurar o 115200 baud na Arduino IDE:
1. **Clique** em **Serial Monitor** (canto superior direito) ou vá em **Tools** -> **Serial Monitor**.
2. **Selecione 115200 baud**.
3. No Serial Monitor, procure o **menu de velocidade** (canto inferior direito) e **selecione 115200**.
4. Digite o comando (**l ou c / listar os registros ou limpar o histórico**).

# 🧩 Desafios durante o desenvolvimento
- Durante o desenvolvimento, um dos problemas foi trabalhar com a memória limitada do Arduino. Para isso, o código utiliza médias das leituras em vez de armazenar todas as amostras, mantém textos fixos na memória Flash e utiliza tipos de variáveis menores quando possível.
- Também foi necessário organizar as informações em uma tela pequena, criar diferentes níveis de alerta e calibrar o sensor de luminosidade para interpretar melhor as condições do ambiente.

# 🎯 Objetivo do projeto
- A proposta da Adega Agnello é reunir monitoramento ambiental, alertas e registro de ocorrências em um único sistema, com configurações simples para o usuário.
- Aplica conhecimentos de programação e eletrônica.
- Trabalhar com sensores, armazenamento de dados, comunicação entre componentes e desenvolvimento de um sistema embarcado.

## 🎓 Sobre o Projeto
Este é um projeto acadêmico desenvolvido para a vinícola fictícia do caso da Vinheria Agnello, como parte da avaliação (CP02 - Checkpoint 02) da FIAP (Faculdade de Informática e Administração Paulista).
- **Disciplina:** Edge Computing & Computer Systems
- **Professor:** Dr. Fábio Henrique Cabrini
- **Vídeo de demonstração:** 🔗[Assista no YouTube](https://youtu.be/xUJtiKg4jtk)
- **Wowki:** 🔗[Projeto no Wowki](https://wokwi.com/projects/478463667788039169)
- **Grupo (CTRL + 5):** **Eduardo** Ambra Giordano de Castro, **Flávia** Sirahata Hasegava e **Lirity** Ribeiro de Paiva.

## 📄 Licença
Este projeto está licenciado sob a licença [MIT](LICENSE).
