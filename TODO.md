# TODO:
* the overall structure of GO game
* currently played games board (server side)
* colours, so that it wont be boring
* themes?
* one player (bot AI agent)/two players
* mouse support? (**ncurses**) (on/off)
* server/client connection
* parralelism (multiple games at once)
* infinite board sizes (well...maybe 32x32 max)
* **server - moves validation, notifications about the games, detection of player (with the state of him, if hes disconnected)**


Architecture:
* use bsd-sockets
* client-server architecture
* client can be created with any programming language 
* Mode/Settings/Exit for client
* Overall stats, notifications, player list on server


# Server example layout

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


# Client example layout


## Board Ideas

### 1
<!-- ########################################################################
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
######################################################################## -->


### 2

<!-- /______________________________________________________________________\
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
\______________________________________________________________________/ -->


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


        