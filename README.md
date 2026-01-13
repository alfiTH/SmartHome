#SmartHome – BLE Sensors Collector

Proyecto de domótica basado en ESP32 + Raspberry Pi, donde sensores BLE envían datos que se recogen en la Raspberry y se almacenan en InfluxDB, visualizándose con Grafana.

## Arquitectura
1. ESP32 emite datos por Bluetooth Low Energy
2. Raspberry Pi escanea BLE y procesa los anuncios
3. InfluxDB almacena las mediciones
4. Grafana visualiza los datos

## Arranque del sistema

Desde la carpeta docker:
```bash
docker compose up -d --build
```

Ver logs del recolector BLE:
```bash
docker compose logs -f ble_collector
```

## Dependencias del ESP32 H2
https://github.com/adafruit/Adafruit_AHTX0
https://github.com/adafruit/ENS160_driver

## Acceso a servicios

[InfluxDB](http://localhost:8086)
[Grafana](http://localhost:3000)

## Problemas conocidos
### Permisos de grafana 
```bash
sudo chown -R 472:472 grafana/data
```
