# SnowRunner Cruise Control

Два самостоятельных проекта для удержания газа в SnowRunner на Windows x64.

| Проект | Запуск | Результат сборки |
| --- | --- | --- |
| [Нативный внутриигровой мод](ingame/INSTALL_NATIVE.md) | Штатный Scaleform HUD, отдельный квадратик F6, загрузка вместе с игрой | `SnowRunnerNativeAutopilot.asi` |
| [Отдельная утилита](standalone/README.md) | EXE со своей иконкой и значком в трее | `standalone/dist/SnowRunnerCruiseControl.exe` |

**F6** включает и выключает удержание газа, **W/S/N** отключают его. Это полный газ, а не поддержание заданной скорости. Выберите один вариант; одновременно запускать оба не нужно.

## Внутриигровой мод

Текущая версия — **[1.1.0: нативный HUD](https://github.com/TRSteep/SR_CruiseControl/releases/tag/ingame-v1.1.0)**. [Инструкция установки и обновления](ingame/INSTALL_NATIVE.md). В готовом ZIP есть ASI, настройки, x64 ASI-загрузчик и лицензии. При переходе с 1.0.0 отключите `SnowRunnerAutopilot.asi`, чтобы старая накладка не загружалась параллельно.

Работа подтверждена в Epic Games, SnowRunner 44.1. Для сборки нужны Windows x64 и MSYS2 UCRT64 GCC. Из PowerShell в корне репозитория:

```powershell
.\ingame\prepare-dependencies.ps1
.\ingame\experimental\scaleform\build.ps1 -Autopilot -Package
```

Исходники нативной версии находятся в `ingame/experimental/scaleform`; этот путь сохранён с этапа разработки. Общая логика управления — `ingame/src/CruiseState.h`. Результат сборки — `ingame/build/scaleform-probe/SnowRunnerNativeAutopilot.asi`; настройки — `SnowRunnerNativeAutopilot.ini`. Проверены F6/W/S/N, Alt+Tab, подсветка, движение и скрытие HUD, отдельная клавиша F6. Поддерживается точная проверенная сборка Epic 44.1; совпадение EXE проверяется по SHA256.

Прежняя версия с DirectX-наложением сохранена в `ingame/src/Autopilot.cpp`, её [архивная документация](ingame/README.md) и установщик относятся к 1.0.0. Нативная версия не использует ImGui, графические перехваты или распознавание изображения. История исследования — [Scaleform integration analysis](docs/scaleform-integration-analysis.md).

## Отдельная утилита

Для сборки достаточно системного компилятора .NET Framework 4.x, без MSYS2 и загрузки зависимостей:

```powershell
.\standalone\build.ps1
```

Запустите `standalone/dist/SnowRunnerCruiseControl.exe`. В `standalone/src` находятся C#-исходники, в `standalone/tests` — проверки. Иконки EXE и трея лежат в `standalone/assets`; их редактируемый векторный источник — `GenerateIcons.cs`. Настройки — `standalone/CruiseControl.ini`. [Запуск и управление](standalone/README.md).

Каждый проект имеет собственные исходники, тесты, скрипт сборки, настройки, документацию и выходную папку сборки. Готовые сборки и промежуточные файлы исключены из Git.
