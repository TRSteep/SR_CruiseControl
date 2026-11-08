# SnowRunner Cruise control (SnowRunner CC) — HUD 1.1.2

Нативный внутриигровой мод для Windows x64. Текущий релиз: **[HUD 1.1.2](https://github.com/TRSteep/SR_CruiseControl/releases/tag/ingame-v1.1.2)**. [Инструкция установки и обновления](INSTALL_NATIVE.md).

Строка «АВТОПИЛОТ» создаётся внутри штатного Scaleform HUD на один шаг выше ручника. Фон, шрифт, наклон, анимация и скрытие используют игровую панель. Справа внутри строки находится векторный спидометр: при включении он становится зелёным, надпись остаётся светлой. Клавиша F6 отображается отдельным штатным квадратиком справа.

Это удержание полного газа, а не поддержание заданной скорости или движение по маршруту. Проверено на Epic Games / SnowRunner 44.1. Точный SHA256 EXE проверяется до установки перехватов; другие сборки не подтверждены.

## Установка и управление

Готовый пакет: `SnowRunnerCC-HUD-ingame-v1.1.2-win-x64.zip`. ASI, INI и совместимый x64 ASI-загрузчик помещаются рядом с `SnowRunner.exe`. Подробные шаги, обновление, диагностика и удаление — в [INSTALL_NATIVE.md](INSTALL_NATIVE.md).

- **F6** — включить/выключить удержание газа.
- **W** — передать газ водителю.
- **S/N** — отключить удержание.
- **Esc, Enter, M, F1, F4**, потеря фокуса — отключить; возврат в игру сам не включает круиз.

Газ должен быть назначен на физическую W, тормоз — на S. Сброс по N относится к физической клавише. Геймпад, руль и выключение двигателя мышью не отслеживаются. Настройка `ToggleKey` меняет также подсказку на HUD.

## Сборка HUD

Из корня репозитория, Windows x64 / MSYS2 UCRT64 GCC:

```powershell
.\ingame\prepare-dependencies.ps1
.\ingame\experimental\scaleform\build.ps1 -Autopilot -Package
```

Исходники — `experimental/scaleform`; общий контроллер — `src/CruiseState.h`. Результат — `build/scaleform-probe/SnowRunnerNativeAutopilot.asi`. [Устройство HUD и проверки](experimental/scaleform/README.md).

Имена `SnowRunnerNativeAutopilot.asi`, `.ini`, `.log` и секция `[Autopilot]` сохранены для совместимости. Название приложения — SnowRunner Cruise control, сокращённо SnowRunner CC. Версия HUD хранится в `VERSION`.

Старые исходники DirectX-наложения в `src/Autopilot.cpp` и скрипты `build.ps1` / `install.ps1` сохранены как история разработки и не собирают текущий HUD. Для актуального релиза используйте команду выше и INSTALL_NATIVE.md.

В релиз включены лицензии [MinHook](https://github.com/TsudaKageyu/minhook/tree/v1.3.4) и [Ultimate ASI Loader 10.0.2](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/tag/v10.0.2). Игровые GFX, шрифты и текстуры не распространяются.
