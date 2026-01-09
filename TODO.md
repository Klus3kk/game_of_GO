# TODO:
* the overall structure of GO game /
* currently played games board (server side) /
* colours, so that it wont be boring /
* themes? / 
* one player (bot AI agent)/two players /
* mouse support? (**ncurses**) (on/off) /
* server/client connection /
* parralelism (multiple games at once) /
* infinite board sizes (well...maybe 32x32 max) /
* **server - moves validation, notifications about the games, detection of player (with the state of him, if hes disconnected)**

# Architecture:
* use bsd-sockets /
* client-server architecture / 
* client can be created with any programming language / 
* Mode/Settings/Exit for client /
* Overall stats, notifications, player list on server

# Current problems
* When player leaves the game, the game is still playing, it should work like if someone leaves unexpectectly then the second player wins and the game is removed /
* Layout is a bit off
* Mouse support still not working
* Nicknames, captures etc should be shown in-game
* Themes should work correctly
* Board is good, but stats should be shown differently because its off
* Everything should be correct with the requirements
* Tidy up the code a bit

# Server example layout

```
|-------------|-----------------------------------|
|   PLAYERS   |               GAMES               |
|             |                                   |
| u         d |                                   |
|-------------|                                   |
|NOTIFICATIONS|                                   |
|             |                                   |
|             |                                   |
|             |                                   |
|             |                                   |
|_____________|___________________________________|
```

# Client example layout


## Board Ideas

### 1
```
########################################################################
#oo# -> white
#oo#
####
#xx# -> black
#xx#
####
#@@# -> select
#@@#
####
#
#
##
#
#
##
#
#
##
#
#
##
#
#
##
#
#
########################################################################
```


### 2

```
/______________________________________________________________________\
|  |
|  |
|__/
|
|
|
|
|
|
|
|
|
|
|
\______________________________________________________________________/
```


## Main menu


            (go big logo)


                PLAY
                SETTINGS
                EXIT

# Settings 

            (go big logo)


            BASIC
            [x] Mouse Support
            [x] Colours
            THEMES
            [ ]
            [ ]
            [ ]
            NICKNAME    

# Play

            (go big logo)


        HOST             JOIN

                START


        
