# SiGameIRL
Устройство для проведения "Своей игры" (SIGame) через "SImulator".

Это версия 0.1. К сборке не рекомендуется. Скоро займусь версией 0.2, где будут учтены проблемы данной версии. В данный момент нажатия кнопок считываются в цикле и бывают моменты, когда два игрока одновременно нажимают кнопки (период между считываниями кнопок 50-100 микросекунд). Во второй версии эта проблема будет решена (правда добавится 4-й провод к кнопкам игроков для отдельного питания светодиодов).

Тестирование платы:
* https://www.youtube.com/watch?v=pi8g3aP3nV4

SiGameIRL собранный (тестирование в игре с 5-ю участниками; там маты, так что 16+):
* https://www.youtube.com/watch?v=th2ahZnA1qU


# Что вам потребуется

## Ведущий

### Для использования с питанием от ПК или блока питания

* Arduino nano (не запаянная)
<dl><img height="60" src="https://github.com/alextrof94/SiGameIRL/blob/master/Other/nano.jpg"></dl>

* Max7219 7-сегментный дисплей
<dl><img height="60" src="https://github.com/alextrof94/SiGameIRL/blob/master/Other/display.jpg"></dl>

* WS2812B светодиодная лента (60 светодиодов/м, 10 светодиодов с длинной 16.7см, можно также использовать и другую плотность светодиодов, общей длинной 16.7см)
<dl><img height="60" src="https://github.com/alextrof94/SiGameIRL/blob/master/Other/ws2812b.jpg"></dl>

* Кнопки 6x6мм с длинным штоком (3 шт)
<dl><img height="60" src="https://github.com/alextrof94/SiGameIRL/blob/master/Other/ButtonJudge.jpg"></dl>

* Резисторы 100-1000 Ом (3 шт)

* Светодиоды (3 шт; желательно красный-желтый-зеленый)

* Макетная плата (для пайки кнопок ведущего и соединителей игроков) 

* Болты м3 (4 шт 6-12мм)

### Для автономного питания от аккумуляторов 18650

* TP4056 с защитой
<dl><img height="60" src="https://github.com/alextrof94/SiGameIRL/blob/master/Other/tp4056.jpg"></dl>

* 18650 держатель (1-2 шт)
<dl><img height="60" src="https://github.com/alextrof94/SiGameIRL/blob/master/Other/holder18650.jpg"></dl>

* Выключатель (10x15мм)
<dl><img height="60" src="https://github.com/alextrof94/SiGameIRL/blob/master/Other/switch.jpg"></dl>

* Болты м3 (2 шт 6-20мм)

## Игрок (Умножаем на количество игроков, x8 обычно, но использовать можно и меньше)

* Кнопка 12x12mm
<dl><img height="60" src="https://github.com/alextrof94/SiGameIRL/blob/master/Other/ButtonPlayer.jpg"></dl>

* Резистор 100-1000 Ом

* Светодиоды (3 шт, можно и 1-2шт; цвет не важен; запаиваются параллельно)

* JST 3 pin
<dl><img height="60" src="https://github.com/alextrof94/SiGameIRL/blob/master/Other/connector.jpg"></dl>

* BLS 3 pin

* PLS 3 pin


# Собранный вид

* Собранный вариант (без питания от аккумуляторов)
<dl><img height="300" src="https://github.com/alextrof94/SiGameIRL/blob/master/Other/assembled.jpg"></dl>
<dl><img height="300" src="https://github.com/alextrof94/SiGameIRL/blob/master/Scheme/Plate.jpg"></dl>

* Кнопка игрока
<dl><img height="300" src="https://github.com/alextrof94/SiGameIRL/blob/master/Other/button.jpg"></dl>
<dl><img height="300" src="https://github.com/alextrof94/SiGameIRL/blob/master/Scheme/ButtonModule.jpg"></dl>

* Внутри
<dl><img height="60" src="https://github.com/alextrof94/SiGameIRL/blob/master/Other/inside.jpg"></dl>

* Соединители игроков
<dl><img height="60" src="https://github.com/alextrof94/SiGameIRL/blob/master/Other/playerConnectors.jpg"></dl>

* Кнопки ведущего
<dl><img height="60" src="https://github.com/alextrof94/SiGameIRL/blob/master/Other/judgeButtons.jpg"></dl>

Все кнопки (сейчас) работают по следующей схеме: 
<dl><img height="60" src="https://github.com/alextrof94/SiGameIRL/blob/master/Other/Arduino-%D0%9A%D0%B0%D0%BA_%D0%BF%D0%BE%D0%B2%D0%B5%D1%81%D0%B8%D1%82%D1%8C_%D1%81%D0%B2%D0%B5%D1%82%D0%BE%D0%B4%D0%B8%D0%BE%D0%B4_%D0%B8_%D0%BA%D0%BD%D0%BE%D0%BF%D0%BA%D1%83_%D0%BD%D0%B0_%D0%BE%D0%B4%D0%B8%D0%BD_%D0%BA%D0%BE%D0%BD%D1%82%D0%B0%D0%BA%D1%82_Arduino.gif"></dl>
