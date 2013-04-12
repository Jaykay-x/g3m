//
//  FloatBuffer_iOS.hpp
//  G3MiOSSDK
//
//  Created by Diego Gomez Deck on 06/09/12.
//
//

#ifndef __G3MiOSSDK__FloatBuffer_iOS__
#define __G3MiOSSDK__FloatBuffer_iOS__

#include "IFloatBuffer.hpp"
#include "ILogger.hpp"

class FloatBuffer_iOS : public IFloatBuffer {
private:
  const int _size;
  float*    _values;
  int       _timestamp;

//  bool         _glBufferBound;
//  unsigned int _glBuffer;

public:
  FloatBuffer_iOS(int size) :
  _size(size),
  _timestamp(0),
  _values(new float[size])
  {
    if (_values == NULL){
      ILogger::instance()->logError("Allocating error.");
    }
  }
  
  FloatBuffer_iOS(float f0,
                  float f1,
                  float f2,
                  float f3,
                  float f4,
                  float f5,
                  float f6,
                  float f7,
                  float f8,
                  float f9,
                  float f10,
                  float f11,
                  float f12,
                  float f13,
                  float f14,
                  float f15) :
  _size(16),
  _timestamp(0)
//  _glBufferBound(false),
//  _glBuffer(0)
  {
    _values = new float[16];
    _values[ 0] = f0;
    _values[ 1] = f1;
    _values[ 2] = f2;
    _values[ 3] = f3;
    _values[ 4] = f4;
    _values[ 5] = f5;
    _values[ 6] = f6;
    _values[ 7] = f7;
    _values[ 8] = f8;
    _values[ 9] = f9;
    _values[10] = f10;
    _values[11] = f11;
    _values[12] = f12;
    _values[13] = f13;
    _values[14] = f14;
    _values[15] = f15;
  }
  
  virtual ~FloatBuffer_iOS();
  
  int size() const {
    return _size;
  }
  
  int timestamp() const {
    return _timestamp;
  }
  
  float get(int i) const {
    
    if (i < 0 || i > _size){
      ILogger::instance()->logError("Buffer Get error.");
    }
    
    return _values[i];
  }
  
  void put(int i,
           float value) {
    
    if (i < 0 || i > _size){
      ILogger::instance()->logError("Buffer Put error.");
    }
    
    if (_values[i] != value) {
      _values[i] = value;
      _timestamp++;
    }
  }
  
  void rawPut(int i,
              float value) {
    
    
    if (i < 0 || i > _size){
      ILogger::instance()->logError("Buffer Put error.");
    }
    
    _values[i] = value;
  }
  
  float* getPointer() const {
    return _values;
  }

//  unsigned int getGLBuffer(int size);

  const std::string description() const;
  
};

#endif
