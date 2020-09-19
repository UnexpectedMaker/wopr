# W.O.P.R. Display

W.O.P.R Display is a modern take on the W.O.P.R. missile launch sequence from the 1983 movie War Games.

I love the movie War Games - It was one of my first glimpses into computers and how they could be connected, and I thought that the W.O.P.R. computer in the movie was amazing, with all of it's flashing LEDs and computer noises.

![codes](http://3sprockets.com.au/um/projects/wopr/wopr_codes.gif "")

But most of all, I loved the intense missile unlock sequence in the movie. W.O.P.R. guessing the unlock code, while Matthew Broderick tries to teach it that no one ever wins. It was exciting and I always wanted to make a "replica" of that unlock sequence... and I finally did!

Build and showcase video
[![video](http://3sprockets.com.au/um/projects/wopr/video_thumbnail.jpg)](https://youtu.be/dfT-OtWHfys)


# Arduino code - New version - Now with more W.O.P.R.!

This repository has the final code for your W.O.P.R. kit. It's written specifically for an ESP32, though not specifically for the TinyPICO, but the kit is designed to have a TinyPICO and Audio Shield plug directly into it.  

To flash the code onto your board, you'll need to have the  Arduino IDE installed and the ESP32 board files installed from the Arduino Board Manager.

This updated version includes:

- New user settings mode that let's you set the GMT, DTS, Brightness and Clock timer (how long it sits on the menu before defaulting to the clock display)
- All menus are now ENUMS so it's easier to follow the code
- Added some of my War Games sound board stuff from: https://www.youtube.com/watch?v=3bC9B00c_XI
- Added support for the extra buttons on the HAXORZ edition of the PCB
- WiFi & NTP connection info on device start 

# Settings Instructions
Button 1 (right button) to switch mode to settings

Button 2 (left button) to select settings mode

Button 1 switches which setting you are on

Button 2 changes the current settings value

HAXORZ edition also uses the 2 new buttons on the back to change settings value up and down 
Hold Button 1 to save settings. Display will show "saving..." when triggered. 

I any of the clock settings are changed (GMT or DTS) then the W.O.P.R. will restart after saving settings.

# Where can I get one?

W.O.P.R is available on tindie

https://www.tindie.com/products/seonr/wopr-missile-launch-code-display-kit/

Wired up for use with the TinyPICO ESP32 Development Board & TinyPICO Audio Shield

All products also available on my own store

http://unexpectedmaker.com/shop/


# Support Unexpected Maker

I love designing, making and releasing our projects as open source. I do it because I believe it’s important to share knowledge and give back to the community, like many have done before me. It helps us all learn and grow.

That said, a lot of time, effort and finances have gone into designing and releasing these files, so please consider supporting me by buy some of my TinyPICO products:

https://tinypico.com/shop

Or by buying one of our products on tindie:

https://www.tindie.com/stores/seonr/

Or by becoming a Patron:

https://www.patreon.com/unexpectedmaker


# Unexpected Maker
http://youtube.com/c/unexpectedmaker

http://twitter.com/unexpectedmaker

https://www.facebook.com/unexpectedmaker/

https://www.instagram.com/unexpectedmaker/
