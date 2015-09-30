/*
 * TileLoader.h
 *
 *  Copyright (c) 2014 Gaeth Cross. Apache 2 License.
 *
 *  This file is part of rviz_satellite.
 *
 *	Created on: 07/09/2014
 */

#ifndef TILELOADER_H
#define TILELOADER_H

#include <QObject>
#include <QImage>
#include <QNetworkAccessManager>
#include <QString>
#include <QNetworkReply>
#include <vector>
#include <tuple>
#include <bitset>
#include <string>   //TODO: CHECK IF NEEDED!

using namespace std;

class TileLoader : public QObject {
  Q_OBJECT
public:
  class MapTile {
  public:
    MapTile(int x, int y, int z, QNetworkReply *reply = 0)
        : x_(x), y_(y), z_(z), reply_(reply) {}
      
    MapTile(int x, int y, int z, QImage & image)
      : x_(x), y_(y), image_(image) {}

    /// X tile coordinate.
    const int &x() const { return x_; }

    /// Y tile coordinate.
    const int &y() const { return y_; }
      
    /// Z tile zoom value.
    const int &z() const { return z_; }

    /// Network reply.
    QNetworkReply *reply() { return reply_; }

    /// Abort the network request for this tile, if applicable.
    void abortLoading();

    /// Has a tile successfully loaded?
    bool hasImage() const;

    /// Image associated with this tile.
    const QImage &image() const { return image_; }
    void setImage(const QImage &image) { image_ = image; }

  private:
    int x_;
    int y_;
    int z_;
    QNetworkReply *reply_;
    QImage image_;
  };

  explicit TileLoader(const std::string &service, double latitude,
                      double longitude, unsigned int zoom, unsigned int blocks,
                      QObject *parent = 0);

  /// Start loading tiles asynchronously.
  void start();

  /// Meters/pixel of the tiles.
  double resolution();

  /// X index of central tile.
  int tileX() const { return tile_x_; }

  /// Y index of central tile.
  int tileY() const { return tile_y_; }

  /// Fraction of a tile to offset the origin (X).
  double originX() const { return origin_x_; }

  /// Fractio of a tile to offset the origin (Y).
  double originY() const { return origin_y_; }

  /// Convert lat/lon to a tile index with mercator projection.
  void latLonToTileCoords_OSM(double lat, double lon, unsigned int zoom,
                                 double &x, double &y);

  void latLonToTileCoords_BING(double lat, double lon, unsigned int zoom,
                                 double &x, double &tileY);

  /// Convert latitude and zoom level to ground resolution.
  double zoomToResolution(double lat, unsigned int zoom) ;

  /// Path to tiles on the server.
  const std::string &objectURI() const { return object_uri_; }

  /// Current set of tiles.
  const std::vector<MapTile> &tiles() const { return tiles_; }

  /// Cancel all current requests.
  void abort();

  double getOffsetx();
  void setOffsetx(double value);

  double getOffsety();
  void setOffsety(double value);

signals:

  void initiatedRequest(QNetworkRequest request);

  void receivedImage(QNetworkRequest request);

  void finishedLoading();

  void errorOcurred(QString description);

public slots:

private slots:

  void finishedRequest(QNetworkReply *reply);

private:

  // Provisional method to switch between OSM[0] and BING[1] tiles
  char provider = 1;

  // Used to handle Bing Tiles
  const double EarthRadius  = 6378137;
  const double MinLatitude  = -85.05112878;
  const double MaxLatitude  = 85.05112878;
  const double MinLongitude = -180;
  const double MaxLongitude = 180;
  double Clip(double n, double minValue, double maxValue);
  unsigned int MapSize(int levelOfDetail);
  string tileToQuad(string x, string y, int startPoint);
  string reverseString(string s);
  double offsetx = 0.0f;
  double offsety = 0.0f;

  /// URI for tile [x,y]
  QUrl uriForTile_OSM(int x, int y) const;
  QUrl uriForTile_BING(std::string quadkey) const;

  /// Maximum number of tiles for the zoom level
  int maxTiles() const;

  double latitude_;
  double longitude_;
  unsigned int zoom_;
  int blocks_;
  int tile_x_;
  int tile_y_;
  double origin_x_;
  double origin_y_;

  QNetworkAccessManager *qnam_;
  QString cachePath_;

  std::string object_uri_;

  std::vector<MapTile> tiles_;
};

#endif // TILELOADER_H
