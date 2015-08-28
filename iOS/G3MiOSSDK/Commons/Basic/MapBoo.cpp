//
//  MapBoo.cpp
//  G3MiOSSDK
//
//  Created by Diego Gomez Deck on 8/21/15.
//
//

#include "MapBoo.hpp"

#include "IG3MBuilder.hpp"
#include "PlanetRendererBuilder.hpp"
#include "ChessboardLayer.hpp"
#include "IDownloader.hpp"
#include "IJSONParser.hpp"
#include "JSONArray.hpp"
#include "JSONObject.hpp"
#include "JSONString.hpp"
#include "JSONNumber.hpp"
#include "URLTemplateLayer.hpp"
#include "MarksRenderer.hpp"
#include "GEOFeature.hpp"
#include "GEO2DPointGeometry.hpp"
#include "Mark.hpp"
#include "IStringBuilder.hpp"

MapBoo::MapBoo(IG3MBuilder* builder,
               const URL&   serverURL,
               MBHandler*   handler,
               bool         verbose) :
_builder(builder),
_serverURL(serverURL),
_handler(handler),
_verbose(verbose)
{
  _layerSet = new LayerSet();
  _layerSet->addLayer( new ChessboardLayer() );

  _builder->getPlanetRendererBuilder()->setLayerSet( _layerSet );

  _markRenderer = new MarksRenderer(false, // readyWhenMarksReady
                                    true,  // renderInReverse
                                    true   // progressiveInitialization
                                    );
  _builder->addRenderer(_markRenderer);

  _vectorStreamingRenderer = new VectorStreamingRenderer(_markRenderer);
  _builder->addRenderer(_vectorStreamingRenderer);

  _downloader  = _builder->getDownloader();
  _threadUtils = _builder->getThreadUtils();
}

MapBoo::~MapBoo() {
  delete _builder;
  delete _handler;
}

void MapBoo::requestMaps(MBMapsHandler* mapsHandler,
                         bool deleteHandler) {
  if (_verbose) {
    ILogger::instance()->logInfo("MapBoo: loading maps");
  }
  _downloader->requestBuffer(URL(_serverURL, "/public/v1/map/"),
                             DownloadPriority::HIGHEST,
                             TimeInterval::zero(),
                             false, // readExpired
                             new MapsBufferDownloadListener(_handler,
                                                            mapsHandler,
                                                            deleteHandler,
                                                            _threadUtils,
                                                            _verbose),
                             true);
}

void MapBoo::MapsBufferDownloadListener::onDownload(const URL& url,
                                                    IByteBuffer* buffer,
                                                    bool expired) {
  if (_verbose) {
    ILogger::instance()->logInfo("MapBoo: downloaded maps");
  }
  _threadUtils->invokeAsyncTask(new MapsParserAsyncTask(_handler,
                                                        _mapsHandler,
                                                        _deleteHandler,
                                                        buffer,
                                                        _verbose),
                                true);
  _mapsHandler = NULL; // moves ownership to MapsParserAsyncTask
}

void MapBoo::MapsBufferDownloadListener::onError(const URL& url) {
  if (_verbose) {
    ILogger::instance()->logInfo("MapBoo: error downloading maps");
  }
  _mapsHandler->onDownloadError();

  if (_deleteHandler) {
    delete _mapsHandler;
    _mapsHandler = NULL;
  }
}

MapBoo::MapsBufferDownloadListener::~MapsBufferDownloadListener() {
  if (_deleteHandler && (_mapsHandler != NULL)) {
    delete _mapsHandler;
  }
#ifdef JAVA_CODE
  super.dispose();
#endif
}

MapBoo::MapsParserAsyncTask::~MapsParserAsyncTask() {
  delete _buffer;

  for (int i = 0; i < _maps.size(); i++) {
    MBMap* map = _maps[i];
    delete map;
  }

  if (_deleteHandler && (_mapsHandler != NULL)) {
    delete _mapsHandler;
  }
#ifdef JAVA_CODE
  super.dispose();
#endif
}

void MapBoo::MapsParserAsyncTask::runInBackground(const G3MContext* context) {
  if (_verbose) {
    ILogger::instance()->logInfo("MapBoo: parsing maps");
  }

  const JSONBaseObject* jsonBaseObject = IJSONParser::instance()->parse(_buffer);

  delete _buffer; _buffer = NULL; // release some memory

  if (jsonBaseObject != NULL) {
    const JSONArray* jsonArray = jsonBaseObject->asArray();
    if (jsonArray != NULL) {
      _parseError = false;

      for (int i = 0; i < jsonArray->size(); i++) {
        MBMap* map = MBMap::fromJSON(_handler, jsonArray->get(i), _verbose );
        if (map == NULL) {
          _parseError = true;
          break;
        }
        _maps.push_back( map );
      }
    }

    delete jsonBaseObject;
  }
}

void MapBoo::MapsParserAsyncTask::onPostExecute(const G3MContext* context) {
  if (_parseError) {
    if (_verbose) {
      ILogger::instance()->logInfo("MapBoo: error parsing maps");
    }

    _mapsHandler->onParseError();
  }
  else {
    if (_verbose) {
      ILogger::instance()->logInfo("MapBoo: parsed maps");
    }

    _mapsHandler->onMaps(_maps);
    _maps.clear(); // moved maps ownership to _handler
  }
}

MapBoo::MBMap* MapBoo::MBMap::fromJSON(MBHandler*            handler,
                                       const JSONBaseObject* jsonBaseObject,
                                       bool verbose) {
  if (jsonBaseObject == NULL) {
    return NULL;
  }

  const JSONObject* jsonObject = jsonBaseObject->asObject();
  if (jsonObject == NULL) {
    return NULL;
  }

  const std::string                         id          = jsonObject->get("id")->asString()->value();
  const std::string                         name        = jsonObject->get("name")->asString()->value();
  std::vector<MapBoo::MBLayer*>             layers      = parseLayers(jsonObject->get("layerSet")->asArray(),
                                                                      verbose);
  std::vector<MapBoo::MBSymbolizedDataset*> symDatasets = parseSymbolizedDatasets(handler,
                                                                                  jsonObject->get("symbolizedDatasets")->asArray(),
                                                                                  verbose );
  const int                                 timestamp   = (int) jsonObject->get("timestamp")->asNumber()->value();

  return new MBMap(id, name, layers, symDatasets, timestamp, verbose);
}

std::vector<MapBoo::MBLayer*> MapBoo::MBMap::parseLayers(const JSONArray* jsonArray,
                                                         bool verbose) {
  std::vector<MapBoo::MBLayer*> result;
  for (int i = 0; i < jsonArray->size(); i++) {
    MBLayer* layer = MBLayer::fromJSON( jsonArray->get(i), verbose );
    if (layer != NULL) {
      result.push_back( layer );
    }
  }
  return result;
}

MapBoo::MBMap::~MBMap() {
  if (_verbose) {
    ILogger::instance()->logInfo("MapBoo: deleting map");
  }

  for (int i = 0; i < _symbolizedDatasets.size(); i++) {
    MBSymbolizedDataset* symbolizedDataset = _symbolizedDatasets[i];
    symbolizedDataset->_release();
  }

  for (int i = 0; i < _layers.size(); i++) {
    MBLayer* layer = _layers[i];
    delete layer;
  }
}

//std::vector<MapBoo::MBDataset*> MapBoo::MBMap::parseDatasets(MBHandler*       handler,
//                                                             const JSONArray* jsonArray,
//                                                             bool verbose) {
//  std::vector<MapBoo::MBDataset*> result;
//  for (int i = 0; i < jsonArray->size(); i++) {
//    MBDataset* dataset = MBDataset::fromJSON(handler, jsonArray->get(i), verbose );
//    if (dataset != NULL) {
//      result.push_back( dataset );
//    }
//  }
//  return result;
//}

std::vector<MapBoo::MBSymbolizedDataset*> MapBoo::MBMap::parseSymbolizedDatasets(MBHandler*       handler,
                                                                                 const JSONArray* jsonArray,
                                                                                 bool verbose) {
  std::vector<MapBoo::MBSymbolizedDataset*> result;
  for (int i = 0; i < jsonArray->size(); i++) {
    MBSymbolizedDataset* symbolizedDataset = MBSymbolizedDataset::fromJSON(handler, jsonArray->get(i), verbose );
    if (symbolizedDataset != NULL) {
      result.push_back( symbolizedDataset );
    }
  }
  return result;
}


MapBoo::MBLayer* MapBoo::MBLayer::fromJSON(const JSONBaseObject* jsonBaseObject,
                                           bool verbose) {
  if (jsonBaseObject == NULL) {
    return NULL;
  }

  const JSONObject* jsonObject = jsonBaseObject->asObject();
  if (jsonObject == NULL) {
    return NULL;
  }

  const std::string type         = jsonObject->get("type")->asString()->value();
  const std::string url          = jsonObject->getAsString("url", "");
  const std::string attribution  = jsonObject->getAsString("attribution", "");

  return new MapBoo::MBLayer(type, url, attribution, verbose);
}

MapBoo::MBLayer::~MBLayer() {
  if (_verbose) {
    ILogger::instance()->logInfo("MapBoo: deleting layer");
  }
}


void MapBoo::MBLayer::apply(LayerSet* layerSet) const {
  if (_type == "URLTemplate") {
    URLTemplateLayer* layer = URLTemplateLayer::newMercator(_url,
                                                            Sector::fullSphere(),
                                                            false,                // isTransparent
                                                            1,                    // firstLevel
                                                            18,                   // maxLevel
                                                            TimeInterval::fromDays(30));

    layerSet->addLayer(layer);
  }
  else {
    ILogger::instance()->logError("MapBoo::MBLayer: unknown type \"%s\"", _type.c_str());
  }
}


void MapBoo::requestMap() {
  if (_verbose) {
    ILogger::instance()->logInfo("MapBoo: requesting map \"%s\"", _mapID.c_str());
  }

  _downloader->requestBuffer(URL(_serverURL, "/public/v1/map/" + _mapID),
                             DownloadPriority::HIGHEST,
                             TimeInterval::zero(),
                             false, // readExpired
                             new MapBufferDownloadListener(this, _handler, _threadUtils, _verbose),
                             true);
}

void MapBoo::setMapID(const std::string& mapID) {
  if (_mapID != mapID) {
    _mapID = mapID;
    requestMap();
  }
}

void MapBoo::setMap(MapBoo::MBMap* map) {
  const std::string mapID = map->getID();
  if (_mapID != mapID) {
    _mapID = mapID;

    applyMap(map);
  }
}

void MapBoo::applyMap(MapBoo::MBMap* map) {
  if (_verbose) {
    ILogger::instance()->logInfo("MapBoo: applying map \"%s\"", map->getID().c_str());
  }

  // clean current map
  _vectorStreamingRenderer->removeAllVectorSets();
  _layerSet->removeAllLayers(true);

  map->apply(_serverURL, _layerSet, _vectorStreamingRenderer);

  // just in case nobody put a layer
  if (_layerSet->size() == 0) {
    _layerSet->addLayer( new ChessboardLayer() );
  }

  if (_handler != NULL) {
    _handler->onSelectedMap(map);
  }

  delete map;
}


void MapBoo::MBMap::apply(const URL&               serverURL,
                          LayerSet*                layerSet,
                          VectorStreamingRenderer* vectorStreamingRenderer) {
  for (int i = 0; i < _layers.size(); i++) {
    MBLayer* layer = _layers[i];
    layer->apply(layerSet);
  }

  for (int i = 0; i < _symbolizedDatasets.size(); i++) {
    MBSymbolizedDataset* symbolizedDataset = _symbolizedDatasets[i];
    symbolizedDataset->apply(serverURL, vectorStreamingRenderer);
  }
}

void MapBoo::MapBufferDownloadListener::onDownload(const URL& url,
                                                   IByteBuffer* buffer,
                                                   bool expired) {
  if (_verbose) {
    ILogger::instance()->logInfo("MapBoo: downloaded map");
  }

  _threadUtils->invokeAsyncTask(new MapParserAsyncTask(_mapboo, _handler, buffer, _verbose),
                                true);
}

void MapBoo::MapBufferDownloadListener::onError(const URL& url) {
  if (_verbose) {
    ILogger::instance()->logInfo("MapBoo: error downloading map");
  }

  _mapboo->onMapDownloadError();
}

MapBoo::MapParserAsyncTask::~MapParserAsyncTask() {
  delete _buffer;
  delete _map;
#ifdef JAVA_CODE
  super.dispose();
#endif
}

void MapBoo::MapParserAsyncTask::runInBackground(const G3MContext* context) {
  if (_verbose) {
    ILogger::instance()->logInfo("MapBoo: parsing map");
  }

  const JSONBaseObject* jsonBaseObject = IJSONParser::instance()->parse(_buffer);

  delete _buffer; _buffer = NULL; // release some memory

  _map = MBMap::fromJSON(_handler, jsonBaseObject, _verbose);

  delete jsonBaseObject;
}

void MapBoo::MapParserAsyncTask::onPostExecute(const G3MContext* context) {
  if (_map == NULL) {
    if (_verbose) {
      ILogger::instance()->logInfo("MapBoo: error parsing map");
    }

    _mapboo->onMapParseError();
  }
  else {
    if (_verbose) {
      ILogger::instance()->logInfo("MapBoo: parsed map");
    }

    _mapboo->onMap(_map);
    _map = NULL; // moved ownership to _mapboo
  }
}

void MapBoo::onMapDownloadError() {
  if (_handler != NULL) {
    _handler->onMapDownloadError();
  }
}

void MapBoo::onMapParseError() {
  if (_handler != NULL) {
    _handler->onMapParseError();
  }
}

void MapBoo::onMap(MapBoo::MBMap* map) {
  applyMap(map);
}

//MapBoo::MBDataset* MapBoo::MBDataset::fromJSON(MBHandler*            handler,
//                                               const JSONBaseObject* jsonBaseObject,
//                                               bool verbose) {
//  if (jsonBaseObject == NULL) {
//    return NULL;
//  }
//
//  const JSONObject* jsonObject = jsonBaseObject->asObject();
//  if (jsonObject == NULL) {
//    return NULL;
//  }
//
//  const std::string        id               = jsonObject->get("id")->asString()->value();
//  const std::string        name             = jsonObject->get("name")->asString()->value();
//  std::vector<std::string> labelingCriteria = jsonObject->getAsArray("labelingCriteria")->asStringVector();
//  std::vector<std::string> infoCriteria     = jsonObject->getAsArray("infoCriteria")->asStringVector();
//  const int                timestamp        = (int) jsonObject->get("timestamp")->asNumber()->value();
//
//  return new MBDataset(handler,
//                       id,
//                       name,
//                       labelingCriteria,
//                       infoCriteria,
//                       timestamp);
//}

MapBoo::MBSymbolizedDataset* MapBoo::MBSymbolizedDataset::fromJSON(MBHandler*            handler,
                                                                   const JSONBaseObject* jsonBaseObject,
                                                                   bool verbose) {
  if (jsonBaseObject == NULL) {
    return NULL;
  }

  const JSONObject* jsonObject = jsonBaseObject->asObject();
  if (jsonObject == NULL) {
    return NULL;
  }

  const std::string        datasetID          = jsonObject->get("datasetID")->asString()->value();
  const std::string        datasetName        = jsonObject->getAsString("datasetName", "");
  const std::string        datasetAttribution = jsonObject->getAsString("datasetAttribution", "");
  std::vector<std::string> labeling           = jsonObject->getAsArray("labeling")->asStringVector();
  const MBShape*           shape              = MBShape::fromJSON( jsonObject->get("shape") );
  std::vector<std::string> info               = jsonObject->getAsArray("info")->asStringVector();



  return new MBSymbolizedDataset(handler,
                                 datasetID,
                                 datasetName,
                                 datasetAttribution,
                                 labeling,
                                 shape,
                                 info);
}

const MapBoo::MBShape* MapBoo::MBShape::fromJSON(const JSONBaseObject* jsonBaseObject) {
  if (jsonBaseObject == NULL) {
    return NULL;
  }

  const JSONObject* jsonObject = jsonBaseObject->asObject();
  if (jsonObject == NULL) {
    return NULL;
  }

  const std::string type = jsonObject->getAsString("type")->value();
  if (type == "Circle") {
    return MBCircleShape::fromJSON(jsonObject);
  }

  ILogger::instance()->logError("Shape type \"%s\" not supported", type.c_str());

  return NULL;
}

const MapBoo::MBCircleShape* MapBoo::MBCircleShape::fromJSON(const JSONObject* jsonObject) {
  const JSONArray* colorArray = jsonObject->getAsArray("color");
  const float red   = (float) colorArray->getAsNumber(0)->value();
  const float green = (float) colorArray->getAsNumber(1)->value();
  const float blue  = (float) colorArray->getAsNumber(2)->value();
  const float alpha = (float) colorArray->getAsNumber(3)->value();

  const int radius = (int) jsonObject->getAsNumber("radius")->value();

  return new MBCircleShape(Color::fromRGBA(red, green, blue, alpha),
                           radius);
}

//MapBoo::MBDataset::~MBDataset() {
//#ifdef JAVA_CODE
//  super.dispose();
//#endif
//}

//void MapBoo::MBDataset::apply(const URL&               serverURL,
//                              VectorStreamingRenderer* vectorStreamingRenderer) const {
//  std::string properties = "";
//  for (int i = 0; i < _labelingCriteria.size(); i++) {
//    properties += _labelingCriteria[i] + "|";
//  }
//  for (int i = 0; i < _infoCriteria.size(); i++) {
//    properties += _infoCriteria[i] + "|";
//  }
//
//  vectorStreamingRenderer->addVectorSet(URL(serverURL, "/public/v1/VectorialStreaming/"),
//                                        _id,
//                                        properties,
//                                        new MBDatasetVectorSetSymbolizer(this),
//                                        true,  // deleteSymbolizer
//                                        DownloadPriority::MEDIUM,
//                                        TimeInterval::zero(),
//                                        true,  // readExpired
//                                        true,  // verbose
//                                        false  // haltOnError
//                                        );
//}

void MapBoo::MBSymbolizedDataset::apply(const URL&               serverURL,
                                        VectorStreamingRenderer* vectorStreamingRenderer) const {
  std::string properties = "";
  for (int i = 0; i < _labeling.size(); i++) {
    properties += _labeling[i] + "|";
  }
  for (int i = 0; i < _info.size(); i++) {
    properties += _info[i] + "|";
  }

  vectorStreamingRenderer->addVectorSet(URL(serverURL, "/public/v1/VectorialStreaming/"),
                                        _datasetID,
                                        properties,
                                        new MBDatasetVectorSetSymbolizer(this),
                                        true,  // deleteSymbolizer
                                        DownloadPriority::MEDIUM,
                                        TimeInterval::zero(),
                                        true,  // readExpired
                                        true,  // verbose
                                        false  // haltOnError
                                        );
}

//const std::string MapBoo::MBDataset::createMarkLabel(const JSONObject* properties) const {
//  const size_t criteriaSize = _labelingCriteria.size();
//  if ((criteriaSize == 0) || (properties->size() == 0)) {
//    return "<label>";
//  }
//  else if (criteriaSize == 1) {
//    return JSONBaseObject::toString( properties->get(_labelingCriteria[0]) );
//  }
//  else {
//    IStringBuilder* labelBuilder = IStringBuilder::newStringBuilder();
//    for (int i = 0; i < criteriaSize; i++) {
//      if (i > 0) {
//        labelBuilder->addString(" ");
//      }
//      const std::string value = JSONBaseObject::toString( properties->get(_labelingCriteria[i]) );
//      labelBuilder->addString( value );
//    }
//
//    const std::string label = labelBuilder->getString();
//    delete labelBuilder;
//    return label;
//  }
//}
//

//const std::string MapBoo::MBDataset::createMarkLabel(const JSONObject* properties) const {
//  const size_t criteriaSize = _labelingCriteria.size();
//  if ((criteriaSize == 0) || (properties->size() == 0)) {
//    return "<label>";
//  }
//  else if (criteriaSize == 1) {
//    return JSONBaseObject::toString( properties->get(_labelingCriteria[0]) );
//  }
//  else {
//    IStringBuilder* labelBuilder = IStringBuilder::newStringBuilder();
//    for (int i = 0; i < criteriaSize; i++) {
//      if (i > 0) {
//        labelBuilder->addString(" ");
//      }
//      const std::string value = JSONBaseObject::toString( properties->get(_labelingCriteria[i]) );
//      labelBuilder->addString( value );
//    }
//
//    const std::string label = labelBuilder->getString();
//    delete labelBuilder;
//    return label;
//  }
//}
//
//
//bool MapBoo::MBFeatureMarkTouchListener::touchedMark(Mark* mark) {
//  _handler->onFeatureTouched(_datasetName, _infoCriteria, _properties);
//  return true;
//}
//
//
//MarkTouchListener* MapBoo::MBDataset::createMarkTouchListener(const JSONObject* properties) const {
//  if (_handler == NULL) {
//    return NULL;
//  }
//
//  const size_t criteriaSize = _infoCriteria.size();
//  if (criteriaSize == 0) {
//    return NULL;
//  }
//
//  JSONObject* infoProperties = new JSONObject();
//  for (int i = 0; i < criteriaSize; i++) {
//    const std::string criteria = _infoCriteria[i];
//    const JSONBaseObject* value = properties->get(criteria);
//    if (value != NULL) {
//      infoProperties->put(criteria, value->deepCopy());
//    }
//  }
//
//  return new MBFeatureMarkTouchListener(_name, _handler, _infoCriteria, infoProperties);
//}

//Mark* MapBoo::MBDataset::createMark(const GEO2DPointGeometry* geometry) const {
//  const GEOFeature* feature = geometry->getFeature();
//  const JSONObject* properties = feature->getProperties();
//  const Geodetic2D position = geometry->getPosition();
//
//  return new Mark(createMarkLabel(properties),
//                  Geodetic3D(position, 0),
//                  ABSOLUTE,
//                  0,                                    // minDistanceToCamera
//                  18,                                   // labelFontSize
//                  Color::newFromRGBA(1, 1, 1, 1),       // labelFontColor
//                  Color::newFromRGBA(0, 0, 0, 1),       // labelShadowColor
//                  NULL,                                 // userData
//                  true,                                 // autoDeleteUserData
//                  createMarkTouchListener(properties),
//                  true                                  // autoDeleteListener
//                  );
//
//  //  return new Mark(URL("file:///icon.png"),
//  //                  Geodetic3D(position, 0),
//  //                  ABSOLUTE,
//  //                  0,
//  //                  NULL,
//  //                  true,
//  //                  createMarkTouchListener(properties),
//  //                  true);
//  
//}

const std::string MapBoo::MBSymbolizedDataset::createMarkLabel(const JSONObject* properties) const {
  const size_t labelingSize = _labeling.size();
  if ((labelingSize == 0) || (properties->size() == 0)) {
    return "<label>";
  }
  else if (labelingSize == 1) {
    return JSONBaseObject::toString( properties->get(_labeling[0]) );
  }
  else {
    IStringBuilder* labelBuilder = IStringBuilder::newStringBuilder();
    for (int i = 0; i < labelingSize; i++) {
      if (i > 0) {
        labelBuilder->addString(" ");
      }
      const std::string value = JSONBaseObject::toString( properties->get(_labeling[i]) );
      labelBuilder->addString( value );
    }

    const std::string label = labelBuilder->getString();
    delete labelBuilder;
    return label;
  }
}


bool MapBoo::MBFeatureMarkTouchListener::touchedMark(Mark* mark) {
  _handler->onFeatureTouched(_datasetName, _info, _properties);
  return true;
}


MarkTouchListener* MapBoo::MBSymbolizedDataset::createMarkTouchListener(const JSONObject* properties) const {
  if (_handler == NULL) {
    return NULL;
  }

  const size_t infoSize = _info.size();
  if (infoSize == 0) {
    return NULL;
  }

  JSONObject* infoProperties = new JSONObject();
  for (int i = 0; i < infoSize; i++) {
    const std::string info = _info[i];
    const JSONBaseObject* value = properties->get(info);
    if (value != NULL) {
      infoProperties->put(info, value->deepCopy());
    }
  }

  return new MBFeatureMarkTouchListener(_datasetName, _handler, _info, infoProperties);
}


Mark* MapBoo::MBSymbolizedDataset::createMark(const GEO2DPointGeometry* geometry) const {
  const GEOFeature* feature    = geometry->getFeature();
  const JSONObject* properties = feature->getProperties();
  const Geodetic2D  position   = geometry->getPosition();

  return new Mark(createMarkLabel(properties),
                  Geodetic3D(position, 0),
                  ABSOLUTE,
                  0,                                    // minDistanceToCamera
                  18,                                   // labelFontSize
                  Color::newFromRGBA(1, 1, 1, 1),       // labelFontColor
                  Color::newFromRGBA(0, 0, 0, 1),       // labelShadowColor
                  NULL,                                 // userData
                  true,                                 // autoDeleteUserData
                  createMarkTouchListener(properties),
                  true                                  // autoDeleteListener
                  );

  //  return new Mark(URL("file:///icon.png"),
  //                  Geodetic3D(position, 0),
  //                  ABSOLUTE,
  //                  0,
  //                  NULL,
  //                  true,
  //                  createMarkTouchListener(properties),
  //                  true);

}
