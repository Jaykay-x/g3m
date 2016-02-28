//
//  Polygon3D.hpp
//  G3MiOSSDK
//
//  Created by Agustin Trujillo Pino on 28/02/16.
//

#ifndef G3MiOSSDK_Polygon3D
#define G3MiOSSDK_Plane

#include "Plane.hpp"


class Polygon3D {

private:
  const Vector3D _v1, _v2, _v3, _v4;
  

public:

#ifdef C_CODE
  const Plane _plane;
#endif
  
#ifdef JAVA_CODE
  private final Plane _plane;
#endif

  
  Polygon3D(const Vector3D& v1,
            const Vector3D& v2,
            const Vector3D& v3,
            const Vector3D& v4):
  _v1(v1),
  _v2(v2),
  _v3(v3),
  _v4(v4),
  _plane(Plane::fromPoints(v1, v2, v3))
  {
  }

};


#endif