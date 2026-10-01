# TransientLock Comp 2 (v2.0)

Compresor con proteccion de transientes (VST3 / AU / AAX / Standalone) hecho con JUCE 8 y C++17.

> **Convive con la version 1:** esta version usa otro `PLUGIN_CODE` (`Tlk2`), otro `BUNDLE_ID` y otro nombre
> (`TransientLock Comp 2`), asi que puedes tener ambas instaladas a la vez en Studio One (u otro DAW).

## Compilar

Requisitos: CMake >= 3.22, compilador C++17 e internet (CMake descarga JUCE 8.0.6).

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j
```

- **Windows:** Visual Studio 2022. Salida en `build/TransientLockComp2_artefacts/Release/VST3`.
  Por defecto copia el plugin a `C:\Program Files\Common Files\VST3` (usa `-DTLC_COPY_AFTER_BUILD=OFF` para evitarlo).
- **macOS:** Xcode. VST3 + AU (universal arm64/x86_64).
- **Linux:** `sudo apt install libasound2-dev libx11-dev libxext-dev libxrandr-dev libxinerama-dev libxcursor-dev libfreetype-dev libfontconfig1-dev libgl1-mesa-dev`
- **AAX:** `-DAAX_SDK_PATH=/ruta/AAX_SDK` (SDK de Avid). Para Pro Tools release hace falta firma PACE/iLok.

Antes de distribuir cambia `COMPANY_NAME`, `PLUGIN_MANUFACTURER_CODE` y `BUNDLE_ID` en `CMakeLists.txt`.

## GitHub Actions

`.github/workflows/build.yml` compila en Windows, macOS y Linux, firma ad-hoc en macOS,
valida el VST3 con **pluginval** (strictness 5) y sube los artefactos. Si haces push de un tag
`vX.Y.Z` (ej. `git tag v1.1.0 && git push --tags`) crea ademas un **GitHub Release** con los binarios.

### Firma de codigo (opcional, para distribuir)
- **Windows:** firma los `.vst3` con `signtool` y un certificado de code signing (guardalo en Secrets).
- **macOS:** firma con tu "Developer ID Application" y notariza con `xcrun notarytool`
  (Apple Developer Program, 99 USD/ano). Sin esto, el usuario debe ejecutar `xattr -cr`.

## Controles

| Control | Funcion |
|---|---|
| Threshold / Ratio / Knee | Curva del compresor de **cuerpo** |
| Attack / Release | Ballistics del compresor de cuerpo |
| **Transient Protection** | Cuanto se libran los transientes de la compresion del *cuerpo* (>75 % anade hasta +2 dB de realce) |
| Body Compression | Cantidad de compresion sobre el cuerpo |
| **Transient Compression** (Amount / Threshold / Ratio / Attack / Release) | Compresor **dedicado a transientes**, con ajustes propios. Solo actua donde el detector marca un transiente. Amount 0 % = desactivado |
| SC HPF | Filtro pasa-altos 2o orden en el detector (20 Hz = off) |
| Sensitivity | Sensibilidad del detector de transientes |
| Input / Makeup / Mix / Output | Ganancias y compresion paralela |
| Oversampling | Off / 2x / 4x (FIR linear-phase; reporta latencia al DAW) |
| Presets | 12 presets de fabrica |

### Control independiente de cuerpo y picos
- **Cuerpo:** Threshold / Ratio / Attack / Release / Knee + Body Compression.
- **Picos:** Transient Compression (Amount, Threshold, Ratio, Attack, Release).
- **Transient Protection** decide cuanto afecta el compresor de *cuerpo* a los picos. Con Protection = 100 %
  el cuerpo no toca los picos y estos solo reciben la compresion del compresor de transientes.
- Consejo: pon el Threshold de Transient Compression *por encima* del nivel del cuerpo para que solo atrape los picos.

La ventana es redimensionable (se recuerda el tamano en el estado del plugin).

## Como funciona

La separacion transient/sustain se hace en el **dominio de ganancia**:

1. Detector diferencial: envolvente rapida (0.5/10 ms) vs lenta (30/200 ms) -> `t` en 0..1 (smoothstep).
2. Compresor feed-forward en dB con soft-knee (detector stereo-linked, con HPF opcional).
3. Reduccion aplicada = `GR_cuerpo x BodyCompression x (1 - Protection x t)  +  GR_transiente x TransientAmount x t`.
4. El detector del compresor baja hasta 6 dB durante el transiente (evita tirones posteriores).

Latencia: 0 muestras con Oversampling Off; con 2x/4x se reporta la del oversampler.

## Medidores
**BODY** (reduccion del cuerpo sin proteccion), **TOTAL** (reduccion total aplicada),
**T-GR** (reduccion del compresor de transientes) y **ACT** (actividad del detector).
