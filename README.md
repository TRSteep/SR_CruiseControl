# SnowRunner Cruise control (SnowRunner CC)

Удержание газа в SnowRunner для Windows x64. Два варианта с отдельными версиями и инструкциями:

| Вариант | Текущая версия | Запуск | Установка |
| --- | --- | --- | --- |
| SnowRunner CC HUD | [1.1.4](https://github.com/TRSteep/SR_CruiseControl/releases/tag/ingame-v1.1.4) | Загружается вместе с игрой, строка внутри штатного Scaleform HUD | [HUD](ingame/INSTALL_NATIVE.md) |
| SnowRunner CC standalone | [1.0.1](https://github.com/TRSteep/SR_CruiseControl/releases/tag/standalone-v1.0.1) | Отдельный EXE со значком в трее | [Standalone](standalone/README.md) |

Полное название приложения — **SnowRunner Cruise control**, сокращённое — **SnowRunner CC**. HUD 1.1.4 добавляет переводы строки, установщик и исправление передачи газа W → F6 → отпускание W. Standalone 1.0.1 использует единое название приложения.

**F6** включает и выключает удержание полного газа, **W/S/N** отключают его. Это удержание газа, а не поддержание заданной скорости или движение по маршруту. Выберите один вариант; одновременно использовать оба не нужно.

## SnowRunner CC HUD 1.1.4

Текущий релиз — [HUD 1.1.4](https://github.com/TRSteep/SR_CruiseControl/releases/tag/ingame-v1.1.4). Доступны установщик EXE, ZIP установщика и отдельный архив для ручной установки.

Штатная строка «КРУИЗ КОНТРОЛЬ» над ручником: векторный спидометр справа становится зелёным при включении, текст остаётся светлым; F6 показана отдельным штатным квадратиком. Строка движется и скрывается вместе с автомобильным HUD. Работа подтверждена на Epic Games / SnowRunner 44.1; мод проверяет точный SHA256 EXE.

Язык надписи выбирается по штатному HUD игры: русский — «КРУИЗ КОНТРОЛЬ», английский — «CRUISE CONTROL», французский — «RÉGULATEUR DE VITESSE». Для остальных языков используется английский. При смене языка надпись обновляется автоматически; длинный текст уменьшается по ширине до значка.

[Описание HUD](ingame/README.md), [установка и обновление](ingame/INSTALL_NATIVE.md), [техническая документация](ingame/experimental/scaleform/README.md).

Сборка из PowerShell в корне репозитория (Windows x64, MSYS2 UCRT64 GCC):

```powershell
.\ingame\prepare-dependencies.ps1
.\ingame\experimental\scaleform\build.ps1 -Autopilot -Package
```

Результат — `ingame/build/scaleform-probe/SnowRunnerNativeAutopilot.asi`. Исходники HUD находятся в `ingame/experimental/scaleform`; путь сохранён с этапа разработки. Общая логика управления — `ingame/src/CruiseState.h`. История исследования — [Scaleform integration analysis](docs/scaleform-integration-analysis.md).

## SnowRunner CC standalone 1.0.1

Отдельная C#-утилита с иконкой EXE и значком состояния в трее. Не требует изменения файлов игры. [Запуск, настройки и ограничения](standalone/README.md).

Сборка системным компилятором .NET Framework 4.x:

```powershell
.\standalone\build.ps1
```

Результат — `standalone/dist/SnowRunnerCruiseControl.exe`; настройки — `CruiseControl.ini`. Исходники находятся в `standalone/src`, проверки — в `standalone/tests`, иконки и их редактируемый источник — в `standalone/assets`.

Имена EXE, ASI, INI, секций настроек и внутренних классов сохранены для совместимости при обновлении. Пользовательское название обоих вариантов — SnowRunner Cruise control (SnowRunner CC). Каждый проект имеет собственные исходники, проверки, настройки и документацию. Готовые сборки и промежуточные файлы исключены из Git.
