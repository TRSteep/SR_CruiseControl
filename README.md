# SnowRunner Cruise Control

Два самостоятельных проекта для удержания газа в SnowRunner на Windows x64.

| Проект | Запуск | Результат сборки |
| --- | --- | --- |
| [Внутриигровой мод](ingame/README.md) | Загружается с игрой, показывает статус рядом с областью АКПП | `ingame/dist/SnowRunnerAutopilot.asi` |
| [Отдельная утилита](standalone/README.md) | EXE со своей иконкой и значком в трее | `standalone/dist/SnowRunnerCruiseControl.exe` |

**F6** включает и выключает удержание газа, **W/S/N** отключают его. Это полный газ, а не поддержание заданной скорости. Выберите один вариант; одновременно запускать оба не нужно.

## Внутриигровой мод

Работа подтверждена в Epic Games, SnowRunner 44.1. Для сборки нужны Windows x64 и MSYS2 UCRT64 GCC. Из PowerShell в корне репозитория:

```powershell
.\ingame\prepare-dependencies.ps1
.\ingame\build.ps1 -RunRenderTests
.\ingame\install.ps1 -GameBin 'D:\Games\SnowRunner\en_us\Sources\Bin'
```

В `ingame/src` находятся исходники мода, в `ingame/tests` — проверки управления и DirectX 11. `ingame/vendor` содержит исходники ImGui/MinHook и лицензии; ASI-загрузчик закреплённой версии скачивается с проверкой SHA256. Настройки — `ingame/SnowRunnerAutopilot.ini`. [Установка, управление и ограничения](ingame/README.md).

## Отдельная утилита

Для сборки достаточно системного компилятора .NET Framework 4.x, без MSYS2 и загрузки зависимостей:

```powershell
.\standalone\build.ps1
```

Запустите `standalone/dist/SnowRunnerCruiseControl.exe`. В `standalone/src` находятся C#-исходники, в `standalone/tests` — проверки. Иконки EXE и трея лежат в `standalone/assets`; их редактируемый векторный источник — `GenerateIcons.cs`. Настройки — `standalone/CruiseControl.ini`. [Запуск и управление](standalone/README.md).

Каждый проект имеет собственные исходники, тесты, скрипт сборки, настройки, документацию и папку `dist`. Готовые сборки и промежуточные файлы исключены из Git.
