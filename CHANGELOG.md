# 0.6.2
- Add more to statistics panel
- Fill sound buffer with zero if no available sound data. This will introduce gaps but will fix latency drift

# 0.6.1
- Add WarmUpTime option to WindowsEvercoastRealtime.ini to fine tune between sync and latency
- Add Video Lag(Networking) to the statistics panel showing the real difference of timestamps between latest decoded video frame and latest received audio frame

# 0.6.0
- Use the latest 0.5.1 plugin for better unlit appearance
- Updated the plugin and associated Blueprints
- Removed most of the image enhancement parameters, replaced with 2 unlit sliders and 1 lit slider

# 0.5.4
- Added "Ignore Audio" option to ignore timestamp driver from the audio, which fixed legacy server's frozen issue

# 0.5.3
- UseOldPicoQuic parameter in Config/Windows/WindowsEvercoastRealtime.ini to control which version of the Intel library to use
- Fix lit/unlit material parameters not being applied on firstr connection

# 0.5.1
- Error message shown when connection is made or failed.
- Manual adjustment parameters now applies to both lit and unlit render mode.

# 0.5.0
- Authentication is added. You'll need a valid access token to playback the realtime stream. Either input in the connect UI, or specify it in Config/DefaultEvercoastRealtime.ini

# 0.4.1
- Adaptive delay compensation option in connection UI
- Static video delay slider in connection UI
- More improvements on audio/video sync in algorithm

# 0.4.0
- Auto rotating arcball view mode
- Arcball view mode now disabled click-to-move
- Audio/video synchornisation improved

# 0.3.8
- Added statistics about dropped video frames
- Better policy to deal with out-of-order video/audio frame pairs, reduce stuttering

# 0.3.6
- Audio interpolate extends to 10 missing packets
- Fixed crashing issue when streaming server stops

# 0.3.5
- Sort audio packets so that avoid incorrect incoming order
- Lost audio packets within 80ms are interpolated
- Fixed audio syncing code
- Statistics on missing audio packets, audio/video sync and video lags are added

# 0.3.0
- Audio improvement by using dual ports
- Manual adjustment for unlit material

# 0.2.0
- Arcball camera movement
- Reposition actor in the scene
- Lit and unlit material

# 0.1.0
- Basic features like connect/disconnect from server
- Move around in FPS fasion
- Toggle statistics panel
- Ability to change between OlympicStudio and Outdoor scene
