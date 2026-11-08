# SnowRunner Cruise control (SnowRunner CC)

[English](#english) · [Русский](#русский)

## English

Hold full throttle in SnowRunner on Windows x64. Crawling through mud can take a long time; SnowRunner CC was made to keep W held without continuously pressing a key or a pedal.

**F6** toggles the throttle hold. **W** takes manual control; **S/N**, the map key **M** and other documented menu keys cancel it. This is a throttle latch, not a target-speed controller or route autopilot.

Choose one version:

| Version | How it works | Documentation |
| --- | --- | --- |
| **Standalone 1.0.1** | Separate portable application with a tray icon. No changes to game files; start it before playing. It does not depend on an exact EXE hash, although game updates may still affect input compatibility. | [Standalone README](standalone/README.md) · [Release](https://github.com/TRSteep/SR_CruiseControl/releases/tag/standalone-v1.0.1) |
| **HUD** | Loads with the game and adds a row to its native interface. Requires the supported game executable; updates and other mods can affect compatibility. | [HUD README](ingame/README.md) · [Release 1.1.5](https://github.com/TRSteep/SR_CruiseControl/releases/tag/ingame-v1.1.5) |

-----------------------
## Русский

Удержание газа в SnowRunner для Windows x64. Попытка сделать удобным долгое перемешивание грязи без педалей: автомобиль медленно, но упорно пробирается вперёд, а постоянно держать W надоедает. SnowRunner CC удерживает газ до отключения водителем.

- **F6** включает и выключает удержание полного газа.
- **W** передаёт газ под ручное управление.
- **S/N** отключают удержание при торможении или переключении двигателя; **M** и другие описанные в инструкции клавиши меню также его отключают.

Это удержание газа, а не поддержание заданной скорости или движение по маршруту. Существуют два варианта — выберите один нужный:

| Вариант | Запуск и особенности | Документация |
| --- | --- | --- |
| **Standalone 1.0.1** | Отдельная переносимая утилита со значком в трее. Установка и изменение файлов игры не требуются; запускать перед игрой. Нет привязки к точному хешу EXE, но совместимость ввода после обновлений всё равно требует проверки. | [README Standalone](standalone/README.md) · [Релиз](https://github.com/TRSteep/SR_CruiseControl/releases/tag/standalone-v1.0.1) |
| **HUD** | Встраиваемый мод: загружается вместе с игрой, строка оформлена как штатный интерфейс. Нужна поддержанная сборка EXE; обновления игры и другие моды могут влиять на совместимость. Есть установщик и ручной пакет. | [README HUD](ingame/README.md) · [Релиз 1.1.5](https://github.com/TRSteep/SR_CruiseControl/releases/tag/ingame-v1.1.5) |

Подробные инструкции запуска, настройки, обновления и сборки находятся в отдельных README каждого проекта. История нативной интеграции — [Scaleform integration analysis](docs/scaleform-integration-analysis.md).
