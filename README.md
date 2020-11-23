# ComProyect - Raspberry Pi

Proyecto ComProyect es un software de análisis multimodal de la comunicación, Este repositorio contiene una parte que se aloja en un dispositivo de recolección de datos creado sobre una placa Raspberry Pi 4.

El proyecto esta escrito con Python 3.8 (captura de audio) y en C (captura de video), donde Python ejecuta el software en C.

## Requerimientos de hardware.

- Raspberry Pi 4 modelo b+ o superior
- ReSpeaker 4-Mic Array
- 4 cámaras usb v4l2
- Grove-Button usa cables 4-Pines

## Requerimientos de software

- Sistema operativo Raspberry Pi OS 32bit
- Python3.8
- Drivers seeed-voicecard

## Instalación

Para instalar en Raspberry pi el Drivers seeed-voicecard ejecute las siguientes líneas de comando.
```
git clone https://github.com/respeaker/seeed-voicecard.git
cd seeed-voicecard
sudo ./install.sh
reboot
```
Puede encontrar más información en sobre el micrófono direccional en [sitio Oficial](https://wiki.seeedstudio.com/ReSpeaker_4_Mic_Array_for_Raspberry_Pi/)

Luego de instalar el drives, clone el proyecto y ejecute en la carpeta raiz el siguiente comando.
```
pip3 install -r requirements.txt
```

Luego queda compilar la solución en C para controlar las cámaras mediante protocolo video4linux2.
```
gcc c/videoStreaming.c -o c/videoStreaming
```
## Configuración

En el archivo service.py puede editar las variables de comunicación con el servidor
```
ip_server = "192.168.1.128" #IP DEL SERVIDOR
portAudio = 5000 #PUERTO DE AUDIO
port1 = 5001 #PUERTO CAMARA 1
port2 = 5002 #PUERTO CAMARA 2
port3 = 5003 #PUERTO CAMARA 3
port4 = 5004 #PUERTO CAMARA 4
```

## Uso

Para iniciar la transmisión utilizar el siguiente comando.

```
python3 service.py
```

## License
[MIT](https://choosealicense.com/licenses/mit/)
