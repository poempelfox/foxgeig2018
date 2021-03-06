
# FoxGeig 2018

<img src="pics/foxgeig2018-fully-assembled.jpg" alt="picture of fully assembled FoxGeig2018" width="500">


## Introduction

This is a geiger counter based on the [MightyOhm Geiger Counter](https://mightyohm.com/blog/products/geiger-counter/) kit. I've removed the original onboard microcontroller and instead used an Adafruit Feather 32U4 RFM69HCW, so that the measurements it makes can now be sent over the air (on 868 MHz). Because I did not want to connect it to the mains, the whole thing is solar powered. Energy is stored in a little LiPo battery, which could easily be connected because the Feather already has the necessary controller for that onboard. I've also constructed a laser-cut "case" made out of 3mm HDF (High-Density Fibreboard), although the term "case" might be a bit misleading here - it's not closed, but mostly a frame to hold the solar panel in place and the rest of the parts together. The radio-transmitted measurements are received by a [Jeelink V3](https://jeelabs.net/projects/hardware/wiki/JeeLink) and fed into [FHEM](https://fhem.de/).

## Part list

* [MightyOhm Geiger Counter](https://mightyohm.com/blog/products/geiger-counter/) kit
* "Lixada Mini 10W Solar Panel" from Amazon. This is basically a little cheap solar panel with USB output.
* Adafruit Feather 32U4 RFM69HCW (868 MHz variant)
* [480 mAh LiPo battery](https://eckstein-shop.de/LiPo-Akku-Lithium-Ion-Polymer-Batterie-37V-480mAh-JST-PH-Connector)
* 3mm HDF and a lasercutter for the case

## Power

The biggest power eater is the MightyOhm, more specifically the circuit that generates the high voltage for the Geiger Tube. Before modification, I measured its power usage at U=3.0V:
* with its onboard microcontroller:  7-8 mA avg, 15 mA peak
* without its microcontroller: 6 mA avg, 6.7 mA peak

It was therefore clear that it wasn't possible to power this from Goldcaps. Instead I bought a little Lithium Polymer battery to attach to the Adafruit Feather, which already comes prepared for that (it has a connector and all necessary electronics for charging onboard). For generating the power, I got a cheap "chinese" solar panel with USB output from Amazon. It claims to do 10W, but that value is probably more of a theoretic maximum. However, it does provide "enough" power (at least in the summer, winter hasn't been tested yet), and if there is enough sun it provides a nicely stable 5V USB power.

## Case

<img src="pics/foxgeig-case.jpg" alt="picture of FoxGeig2018 case" width="500">

The parts for the "case" or frame were cut out of 3mm thick HDF in the [FAU Fablab](https://fablab.fau.de). They were then glued together, and after the glue had dried, the whole thing got coated with some spray wood coating.

The files for the case can be found in the "case" subdirectory in this repo.

## FAQ

Well they're not really asked _frequently_ but I'll answer them anyways.

### Why?

Why not?
Not everybody can claim to have an Geiger Counter on their balcony. Can you?

### Is it useful?

Well, it can be used to generate nice graphs that show the (natural) ambient radiation on my balcony in Erlangen, Germany. For example, on 2018-04-24 it looked like this:
![RRD graph CPM on my balcony 2018-04-24](pics/rrd-geigercounter-day.png)

However, if you intend to use this or the values it measures for anything serious, you're out of your mind. This is a toy, not a device to get reliable measurements.
