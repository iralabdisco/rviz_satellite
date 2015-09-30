/*
 * TileLoader.cpp
 *
 *  Copyright (c) 2014 Gaeth Cross. Apache 2 License.
 *
 *  This file is part of rviz_satellite.
 *
 *	Created on: 07/09/2014
 */

#include "tileloader.h"

#include <QUrl>
#include <QNetworkRequest>
#include <QVariant>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QImageReader>
#include <stdexcept>
#include <boost/regex.hpp>
#include <ros/ros.h>
#include <ros/package.h>

#define STI(a) ((a) == '1' ? (1) : (0))

size_t replaceRegex(const boost::regex &ex, std::string &str,
                           const std::string &replace) {
  std::string::const_iterator start = str.begin(), end = str.end();
  boost::match_results<std::string::const_iterator> what;
  boost::match_flag_type flags = boost::match_default;
  size_t count = 0;
  while (boost::regex_search(start, end, what, ex, flags)) {
    str.replace(what.position(), what.length(), replace);
    start = what[0].second;
    count++;
  }
  return count;
}

void TileLoader::MapTile::abortLoading() {
  if (reply_) {
    reply_->abort();
    reply_ = 0;
  }
}

bool TileLoader::MapTile::hasImage() const { return !image_.isNull(); }

TileLoader::TileLoader(const std::string &service, double latitude,
                       double longitude, unsigned int zoom, unsigned int blocks,
                       QObject *parent)
    : QObject(parent), latitude_(latitude), longitude_(longitude), zoom_(zoom),
      blocks_(blocks), qnam_(0), object_uri_(service) {
  assert(blocks_ >= 0);
          
    QString packagePath = QString::fromStdString(ros::package::getPath("rviz_satellite"));
    QString folderName("mapscache");
    cachePath_ = QDir::cleanPath(packagePath + QDir::separator() + folderName);
    QDir dir(cachePath_);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
          
  /// @todo: some kind of error checking of the URL

  //  calculate center tile coordinates
  double x, y;
  latLonToTileCoords_OSM (latitude_, longitude_, zoom_, x, y);
  latLonToTileCoords_BING(latitude_, longitude_, zoom_, x, y);
  tile_x_ = std::floor(x);
  tile_y_ = std::floor(y);
  //  fractional component
  origin_x_ = x - (double)tile_x_;
  origin_y_ = y - (double)tile_y_;

  ROS_ERROR_STREAM ("ORIGIN: " << origin_x_ << "\t" << origin_y_);
}

void TileLoader::start() {
  //  discard previous set of tiles and all pending requests
  abort();

  qnam_ = new QNetworkAccessManager(this);
  QObject::connect(qnam_, SIGNAL(finished(QNetworkReply *)), this,
                   SLOT(finishedRequest(QNetworkReply *)));

  //  determine what range of tiles we can load
  const int min_x = std::max(0, tile_x_ - blocks_);
  const int min_y = std::max(0, tile_y_ - blocks_);
  const int max_x = std::min(maxTiles(), tile_x_ + blocks_);
  const int max_y = std::min(maxTiles(), tile_y_ + blocks_);

  //  initiate requests
  for (int y = min_y; y <= max_y; y++) {
    for (int x = min_x; x <= max_x; x++) {
        
        std::bitset<32> xbin(x);
        std::bitset<32> ybin(y);
        QString fileName;
        QString fullPath;
        string quadkey;

        if (provider == 0) //OSM
        {
            // Generate filename
            fileName = "x_" + QString::number(x)+ "y_" +QString::number(y) +"z_" +QString::number(zoom_)+ ".jpg";
            fullPath = QDir::cleanPath(cachePath_ + QDir::separator() + fileName);
        }
        else if (provider == 1) //BING
        {
            unsigned int startPoint = 32 - (floor(log(std::max(x,y))/log(2)) + 1) ;
            ROS_DEBUG_STREAM("Start Point: " << startPoint);
            ROS_ASSERT(startPoint >=0);
            quadkey = tileToQuad(xbin.to_string(), ybin.to_string(), startPoint);

            //TODO: set filename
        }
        else
        {
            // TODO: handle this situation
            abort();
            return;
        }

        // Check if tile is already in the cache [common]
        QFile tile(fullPath);
        if (tile.exists())
        {
            QImage image(fullPath);
            tiles_.push_back(MapTile(x, y, zoom_, image));
        }
        else
        {
            QUrl uri ;
            if (provider==0) //OSM
                uri = uriForTile_OSM(x, y);
            if (provider==1) //BING
                uri = uriForTile_BING(quadkey);

            //  send request
            const QNetworkRequest request = QNetworkRequest(uri);
            QNetworkReply *rep = qnam_->get(request);
            emit initiatedRequest(request);
            tiles_.push_back(MapTile(x, y, zoom_, rep));

            ROS_DEBUG_STREAM("Downloading tile:\t" << QString(uri.toString()).toUtf8().constData());
        }
    }
  }

    // check if we are finished already
    bool loaded = true;
    for (MapTile &tile : tiles_) {
        if (!tile.hasImage()) {
            loaded = false;
            std::cout << "Tile x: " << tile.x() << " y: "<< tile.y() << " has no image new version"<< std::endl;
        }
    }
    if (loaded) {
        emit finishedLoading();
    }
}

double TileLoader::resolution()
{
  return zoomToResolution(latitude_, zoom_);
}

/// @see http://wiki.openstreetmap.org/wiki/Slippy_map_tilenames
/// For explanation of these calculations.
void TileLoader::latLonToTileCoords_OSM(double lat, double lon, unsigned int zoom,
                                    double &x, double &y) {
  if (zoom > 19) {
    throw std::invalid_argument("Zoom level " + std::to_string(zoom) +
                                " too high");
  } else if (lat < -85.0511 || lat > 85.0511) {
    throw std::invalid_argument("Latitude " + std::to_string(lat) + " invalid");
  } else if (lon < -180 && lon > 180) {
    throw std::invalid_argument("Longitude " + std::to_string(lon) +
                                " invalid");
  }

  const double rho = M_PI / 180;
  const double lat_rad = lat * rho;

  unsigned int n = (1 << zoom);
  x = n * ((lon + 180.0f) / 360.0f);
  y = n * (1.0f - (std::log(std::tan(lat_rad) + 1 / std::cos(lat_rad)) / M_PI)) /
      2.0f;
  std::cout << "Center tile coords: " << x << ", " << y << std::endl;
}

/// @see https://msdn.microsoft.com/en-us/library/bb259689.aspx
/// For explanation of these calculations.
void TileLoader::latLonToTileCoords_BING(double lat, double lon, unsigned int zoom,
                                    double &tileX, double &tileY) {
    if (zoom > 19)
    {
        throw std::invalid_argument("Zoom level " + std::to_string(zoom) +
                                    " too high");
    }
    else if (lat < -85.05112878f || lat > 85.05112878f)
    {
        throw std::invalid_argument("Latitude " + std::to_string(lat) + " invalid");
    }
    else if (lon < -180.0f && lon > 180.0f)
    {
        throw std::invalid_argument("Longitude " + std::to_string(lon) +
                                    " invalid");
    }

    //LatLongToPixelXY
    double latitude  = Clip(lat, MinLatitude, MaxLatitude);
    double longitude = Clip(lon, MinLongitude, MaxLongitude);
    ROS_ERROR_STREAM("Clipping latlon  \t"<<latitude<<"\t"<<longitude);
    double x = (longitude + 180.0f) / 360.0f;
    double sinLatitude = sin(latitude * M_PI / 180.0f);
    double y = 0.5f - log((1.0f + sinLatitude) / (1.0f - sinLatitude)) / (4.0f * M_PI);
    uint mapSize = MapSize(zoom);
    double pixelX = (int) Clip(x * mapSize + 0.5f, 0.0f, mapSize - 1.0f);
    double pixelY = (int) Clip(y * mapSize + 0.5f, 0.0f, mapSize - 1.0f);
    ROS_ERROR_STREAM("LatLongToPixelXY \t"<<(int)pixelX<<"\t"<<(int)pixelY);

    //PixelXYToTileXY
    tileX = floor(pixelX / 256.0f); //tileX
    tileY = floor(pixelY / 256.0f); //tileY
    ROS_ERROR_STREAM("PixelXYToTileXY  \t"<<x<<"\t"<<y);

    //TileXYToPixelXY
    double pixelX2 = tileX * 256.0f;
    double pixelY2 = tileY * 256.0f;
    ROS_ERROR_STREAM("TileXYToPixelXY  \t"<<(int)pixelX<<"\t"<<(int)pixelY);

    //Get offset and multiply by the Ground Resolution
    double GroundResolution = cos(latitude * M_PI / 180.0f) * 2.0f * M_PI * EarthRadius / MapSize(zoom);
    ROS_ERROR_STREAM("GroundResolution  \t"<<GroundResolution);

    //handle offset from lat/lon to the corner of the quadtile
    setOffsetx(-(pixelX-pixelX2)*GroundResolution);
    setOffsety( (pixelY-pixelY2)*GroundResolution);

    ROS_ERROR_STREAM(x<<"\t"<<(int)pixelX);
    ROS_ERROR_STREAM(y<<"\t"<<(int)pixelY);

    ROS_ERROR_STREAM("Calculated Offset\t" << (int)(pixelX-pixelX2) << "\t"<< (int)(pixelY-pixelY2) << "\t" << getOffsetx()<<"\t"<<getOffsety());
}


double TileLoader::zoomToResolution(double lat, unsigned int zoom) {
  double lat_rad = lat * M_PI / 180;
  return 156543.034 * std::cos(lat_rad) / (1 << zoom);
}

void TileLoader::finishedRequest(QNetworkReply *reply) {
  const QNetworkRequest request = reply->request();

  //  find corresponding tile
  MapTile *tile = 0;
  for (MapTile &t : tiles_) {
    if (t.reply() == reply) {
      tile = &t;
    }
  }
  if (!tile) {
    //  removed from list already, ignore this reply
    return;
  }

  if (reply->error() == QNetworkReply::NoError) {
    //  decode an image
    QImageReader reader(reply);
    if (reader.canRead()) {
      QImage image = reader.read();
      tile->setImage(image);
      QString fileName = "x_" + QString::number(tile->x())+ "y_" +QString::number(tile->y()) +"z_" +QString::number(tile->z())+ ".jpg";
      QString fullPath = QDir::cleanPath(cachePath_ + QDir::separator() + fileName);
      image.save(fullPath,"JPEG");
      emit receivedImage(request);
    } else {
      //  probably not an image
      QString err;
      err = "Unable to decode image at " + request.url().toString();
      emit errorOcurred(err);
    }
  } else {
    QString err;
    err = "Failed loading " + request.url().toString();
    err += " with code " + QString::number(reply->error());
    emit errorOcurred(err);
  }

  //  check if all tiles have images
  bool loaded = true;
  for (MapTile &tile : tiles_) {
    if (!tile.hasImage()) {
      loaded = false;
    }
  }
  if (loaded) {
    emit finishedLoading();
  }
}

double TileLoader::Clip(double n, double minValue, double maxValue)
{
    return std::min(std::max(n,minValue),maxValue);
}

unsigned int TileLoader::MapSize(int levelOfDetail)
{
    return (unsigned int) 256 << levelOfDetail; // C# to C++ Bing
}

QUrl TileLoader::uriForTile_OSM(int x, int y) const {
  std::string object = object_uri_;
  //  place {x},{y},{z} with appropriate values
  replaceRegex(boost::regex("\\{x\\}", boost::regex::icase), object,
               std::to_string(x));
  replaceRegex(boost::regex("\\{y\\}", boost::regex::icase), object,
               std::to_string(y));
  replaceRegex(boost::regex("\\{z\\}", boost::regex::icase), object,
               std::to_string(zoom_));

  const QString qstr = QString::fromStdString(object);
  return QUrl(qstr);
}

QUrl TileLoader::uriForTile_BING(std::string quadkey) const {
  std::string object = object_uri_;
  //  place {x},{y},{z} with appropriate values
  replaceRegex(boost::regex("\\{quad\\}", boost::regex::icase), object,
               quadkey);
  const QString qstr = QString::fromStdString(object);
  return QUrl(qstr);
}

int TileLoader::maxTiles() const { return (1 << zoom_) - 1; }

void TileLoader::abort() {
  tiles_.clear();

  //  destroy network access manager
  if (qnam_) {
    delete qnam_;
    qnam_ = 0;
  }
}

string TileLoader::tileToQuad(string x, string y, int startPoint)
{
    string bin="";
    string quad="";
    int oct = 0;

    ROS_INFO_STREAM(x);
    ROS_INFO_STREAM(y);
    ROS_INFO_STREAM(startPoint);

    for(int i=startPoint; i<32; i++)
    {
        bin = bin + y[i];
        bin = bin + x[i];
    }
    for (int i=bin.length()-1; i>=0; i-=2)
    {
        oct = STI(bin[i]) + STI(bin[i-1]) * 2;
        quad = quad + std::to_string(oct);
    }
    return reverseString(quad);
}

string TileLoader::reverseString(string s)
{
    string rev;
    for(uint i=0;i<s.length();i++) {
        rev = rev + s[s.length()-i-1];
    }
    return rev;
}
double TileLoader::getOffsety()
{
    return offsety;
}

void TileLoader::setOffsety(double value)
{
    offsety = value;
}

double TileLoader::getOffsetx()
{
    return offsetx;
}

void TileLoader::setOffsetx(double value)
{
    offsetx = value;
}


