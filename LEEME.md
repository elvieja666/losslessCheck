# Lossless Check

Plugin para Logic (AU y VST3, Mac) que **escucha un audio y te dice si es original o si tiene la huella de un MP3/AAC**, por ejemplo un "falso WAV" hecho desde un MP3. Lo ponés en la pista, le das Play y espera a estar seguro antes de dar el veredicto.

El audio pasa sin tocarse: el plugin solo escucha.

---

El plugin se compila en GitHub, en una Mac en la nube: vos no instalás nada de desarrollo. Funciona en Mac con Apple Silicon e Intel.

## Instalar

1. En la página del repositorio, a la derecha en **Releases**, tocá **Lossless Check (última versión)** y bajá **LosslessCheck-mac.zip**. Descomprimilo con doble clic.
2. Abrí la **Terminal** (Cmd + Espacio, escribí "Terminal"), pegá esto y apretá Enter:

```
bash ~/Downloads/LosslessCheck-mac/instalar.command
```

3. Cuando dice **✅ Listo**, abrí Logic. Lo encontrás en **Audio Units > 0600musica > Lossless Check**.

## Cómo usarlo

1. Poné Lossless Check en la pista del audio que querés revisar.
2. Dale Play y dejalo sonar. Mientras escucha dice **Analizando...** con una barra de progreso.
3. Cuando está seguro, da el veredicto.

Cada vez que le das **Play** empieza un análisis nuevo. También se reinicia solo después de 2 segundos de silencio, y si cambia el audio mientras suena (por ejemplo, si pasás de un WAV a un MP3 con solo o mute). El botón **Reiniciar** fuerza un análisis nuevo.

## Los veredictos

| | Veredicto | Qué quiere decir | Cuándo aparece |
|---|---|---|---|
| 🟢 | **Original** · Sin huella de MP3/AAC | Los agudos llegan arriba y son continuos, como en un WAV original. | A los ~30 s con música |
| 🟠 | **Comprimido** · MP3/AAC · posible 256–320 kbps | Tiene la huella de un MP3 o AAC. **Si el archivo es WAV o AIFF, es un falso WAV.** | Entre 10 y 20 s; la calidad a los ~45 s |
| 🟠 | **Alta resolución inflada** | Un archivo de 88.2/96 kHz que en realidad viene de uno de 44.1/48 kHz. | A los ~10 s |
| 🟡 | **Dudoso** | Muy pocas huellas para asegurarlo. Dejalo sonar más. | A los ~45 s |
| 🔵 | **No se puede analizar** | La música casi no llega a los agudos (tema muy oscuro), que es donde se ve la huella. | A los ~30 s |

El botón **Detalles** muestra los datos técnicos: hasta dónde llega la señal y cuántos "huecos" tienen los agudos.

### Cómo se da cuenta

Un MP3 o AAC guarda los agudos con pocos bits, y cuando no le alcanzan los **borra de a ratos**, en pedacitos de ~12 ms, sobre todo en el estéreo. Un WAV original nunca hace eso. Los MP3 de baja calidad además **cortan** los agudos por arriba (por ejemplo en 16 kHz). El plugin mide las dos cosas.

### Sobre la calidad estimada

Es una **estimación**, por eso se muestra como rango (por ejemplo "256–320 kbps"). Sale de cuántos huecos tienen los agudos o, si no llega a los agudos, de dónde corta. Está calibrada con un master 24/48 pasado por el MP3 de Logic (96 a 320), LAME (128 a 320, V0 y V2) y AAC; con esos 17 archivos no se equivocó en ningún veredicto y el rango acertó a partir de un minuto de escucha. Con más ejemplos de otros temas y codificadores se va a ir afinando.

### Tené en cuenta

- El plugin no sabe si el archivo es WAV o MP3: Logic le pasa audio, no el archivo. Te dice si el audio tiene huella de MP3/AAC; vos sabés qué archivo estás escuchando.
- Un falso WAV hecho desde un MP3 de muy alta calidad deja pocas huellas: por eso existe "Dudoso".
- El análisis corre mientras la ventana del plugin está abierta.

## Cuando haya una versión nueva

Volvé a bajar **LosslessCheck-mac.zip** de Releases y corré el mismo comando de instalación.
