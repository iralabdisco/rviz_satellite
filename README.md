## rviz_satellite

** THIS IS AN EXPERIMENTAL VERSION FOR USING THE BING MAPS SYSTEM **

Plugin for rviz for displaying satellite maps loaded from the internet.

![Alt text](.screenshot.png?raw=true "Example Image")

### Usage

In order to use rviz_satellite, add this package to your catkin workspace. Then add an instance of `AerialMapDisplay` to your rviz config.

The `Topic` field must point to a publisher of `sensor_msgs/NavSatFix`. Note that rviz_satellite will not repeatedly reload the same GPS coordinates twice (unless you modify something in the GUI options).

You must provide an `Object URI` (or URL) from which the satellite images are loaded. rviz_satellite presently only supports the [OpenStreetMap](http://wiki.openstreetmap.org/wiki/Slippy_map_tilenames) convention for tile names.

The URI should have the form:

``http://otile1.mqcdn.com/tiles/1.0.0/sat/{z}/{x}/{y}.jpg``

This is the default URI, which will load data from MapQuest. The tiles are free, and go up to zoom level 18. For higher zoom levels, consider using [MapBox](https://www.mapbox.com).

Map tiles will be cached to the `mapscache` directory in the `rviz_satellite` package directory. At present the cache does not expire automatically - you should delete the files in the folder if you want the images to be reloaded.


============================
This version allows to use also the tiles provided by Bing Maps. Up to now I'm using this URI, but expect changes.

``http://ecn.t1.tiles.virtualearth.net/tiles/a{quad}.jpeg?g=3653``

The code was modified using the information provided here: ``https://msdn.microsoft.com/en-us/library/bb259689.aspx``

Using OSM or BING is still hard-coded, so you need to modify the ``provider`` variable in tileloader.h.
============================


### Options

- `Alpha` is simply the display transparency.
- `Draw Under` will cause the map to be displayed below all other geometry.
- `Zoom` is the zoom level of the map. Recommended values are 16-19, as anything smaller is _very_ low resolution. 19 is the current max.
- `Blocks` number of adjacent blocks to load. rviz_satellite will load the central block, and this many blocks around the center. 6 is the current max.

### Questions, Bugs

Contact the author, or open an issue.

### Credits 
based on the extraordinary work by gareth-cross on github
mods by trigal on github
