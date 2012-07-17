//
//  SimpleTileTexturizer.cpp
//  G3MiOSSDK
//
//  Created by Diego Gomez Deck on 12/07/12.
//  Copyright (c) 2012 IGO Software SL. All rights reserved.
//

#include "SimpleTileTexturizer.hpp"

#include "Context.hpp"
#include "TextureMapping.hpp"


TilePetitions SimpleTileTexturizer::registerTilePetitions(const Tile* tile)
{
  
  std::string url = "http://www.arkive.org/images/browse/world-map.jpg"; //FIXED

  //SAVING PETITION
  TilePetitions tt(tile);
  tt.add(url, tile->getSector());
  _tilePetitions.push_back(tt);
  
  return tt;
}

std::vector<MutableVector2D> SimpleTileTexturizer::createTextureCoordinates() const
{
  std::vector<MutableVector2D> texCoor;

  const double lonRes1 = (double) (_resolution-1), latRes1 = (double) (_resolution-1);
  for(double i = 0.0; i < _resolution; i++){
    double u = (i / lonRes1);
    for (double j = 0.0; j < _resolution; j++) {
      const double v = 1.0 - (j / latRes1);
      MutableVector2D v2d(u,v);
      texCoor.push_back(v2d);
    }
  }
  
  return texCoor;
}

Mesh* SimpleTileTexturizer::getMesh(const RenderContext* rc,
              const Tile* tile,
              Mesh* mesh)
{

  int __JM_at_work;     
  //CHECKING IF THE TILE IS COMPLETED
  for (int i = 0; i < _tilePetitions.size(); i++) { //EACH TILE
    if (_tilePetitions[i].getTile()== tile){
      
      if (_tilePetitions[i].allFinished())
      {
        //TAKING JUST FIRST!!!
        const ByteBuffer* bb = _tilePetitions[i].getPetitions()[0].getByteBuffer();
        IImage *im = rc->getFactory()->createImageFromData(*bb);
        
        const std::string& url = _tilePetitions[i].getPetitions()[0].getURL();   
        int texID = rc->getTexturesHandler()->getTextureId(rc, im, url, 256, 256);
        
        //new TextureMapping(texID, createIndex());
      } else{
        //NOT ALL IMAGES ARE DOWNLOADED
        return mesh;
      }
      
    }
  }
}

Mesh* SimpleTileTexturizer::texturize(const RenderContext* rc,
                                      const Tile* tile,
                                      Mesh* mesh) {
  
  int _todo;
  return mesh; //UNTIL IS FINISHED
  
  //THROWING AND CREATING THE PETITIONS
  int priority = 10;
  TilePetitions tp = registerTilePetitions(tile);
  Downloader* d = rc->getDownloader();
  for (int i = 0; i < tp.getPetitions().size(); i++) {
    const std::string& url = tp.getPetitions()[i].getURL();
    d->request(url, priority, (IDownloadListener*) this);
  }
  
  

  
  
  return getMesh(rc, tile, mesh);
}

void SimpleTileTexturizer::onDownload(const Response &response){
  
  std::string url = response.getURL().getPath();
  for (int i = 0; i < _tilePetitions.size(); i++) { //EACH TILE
    
    for (int j = 0; j < _tilePetitions[i].getPetitions().size(); j++) {
      if (_tilePetitions[i].getPetitions()[j].getURL() == url)
      {
        ByteBuffer *bb = new ByteBuffer(*response.getByteBuffer());
        
        //STORING PIXEL DATA FOR RECEIVED URL
        _tilePetitions[i].getPetitions()[j].setByteBuffer(bb);
      }
    }
  }
}


void SimpleTileTexturizer::onError(const Response& e)
{
  
}
