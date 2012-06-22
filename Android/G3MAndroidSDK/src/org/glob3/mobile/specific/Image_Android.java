package org.glob3.mobile.specific;

import org.glob3.mobile.generated.IImage;

import android.graphics.Bitmap;

public class Image_Android extends IImage{
	
	final private Bitmap _image;
	
	public Image_Android(Bitmap image) {
		_image = image;
	}
	
	public Bitmap getBitmap() {
		return _image;
	}

}
