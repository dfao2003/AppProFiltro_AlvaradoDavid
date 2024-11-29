package com.example.apppro;

import androidx.appcompat.app.AppCompatActivity;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.View;
import android.widget.SeekBar;

import com.example.apppro.databinding.ActivityMainBinding;
import com.longdo.mjpegviewer.MjpegView;


public class MainActivity extends AppCompatActivity {
    // Used to load the 'apppro' library on application startup.
    static {
        System.loadLibrary("apppro");
    }

    MjpegView viewer;
    Handler handler = new Handler(Looper.getMainLooper());
    private static final long CAPTURE_INTERVAL = 0;
    private android.widget.ImageView original, bordes;
    private android.widget.Button boton, boton2, botonAllColors;
    private SeekBar seekBar;
    private float angleFactor = 2 * (float)Math.PI; // El factor inicial

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        //original = findViewById(R.id.imageView);
        bordes = findViewById(R.id.imageView);
        boton = findViewById(R.id.button);
        boton2 = findViewById(R.id.button2);

        boton.setOnClickListener(new View.OnClickListener(){
            @Override
            public void onClick(View view){
                viewer = findViewById(R.id.mjpegview);
                viewer.setMode(MjpegView.MODE_FIT_WIDTH);
                viewer.setAdjustHeight(true);
                viewer.setSupportPinchZoomAndPan(true);
                viewer.setRecycleBitmap(true);
                viewer.setUrl("http://192.168.152.62:81/stream");
                viewer.startStream();

                Frames();
            }
        });

        boton2.setOnClickListener(new View.OnClickListener(){
            @Override
            public void onClick(View view){
               viewer.stopStream();
            }
        });

    }

    private void Frames(){
        handler.post(new Runnable(){
            @Override
            public void run(){
                Bitmap frame = captureFrame();
                if(frame != null){
                    procBitmap(frame);
                }
                handler.postDelayed(this, CAPTURE_INTERVAL);
            }
        });
    }

    private Bitmap captureFrame(){
        viewer.setDrawingCacheEnabled(true);
        viewer.buildDrawingCache();
        Bitmap bitmap = Bitmap.createBitmap(viewer.getDrawingCache());
        viewer.setDrawingCacheEnabled(false);
        return bitmap;
    }

    private void procBitmap(Bitmap bitmap){
        Bitmap bitmap2 = bitmap.copy(bitmap.getConfig(), true);
        detectorBordes(bitmap,bitmap2);
        bordes.setImageBitmap(bitmap2);
    }

    @Override
    protected void onPause() {
        super.onPause();
        if (viewer != null) {
            viewer.stopStream(); // Detener el stream cuando la app no está visible
        }
    }

    @Override
    protected void onResume() {
        super.onResume();
        if (viewer != null) {
            viewer.startStream(); // Reiniciar el stream cuando la app vuelve a ser visible
        }
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        if (viewer != null) {
            viewer.stopStream(); // Asegurarte de detener el stream al salir de la app
        }
    }

    /**
     * A native method that is implemented by the 'apppro' native library,
     * which is packaged with this application.
     */
    public native String stringFromJNI();
    // Método para pasar el valor al código C++
    public native void detectorBordes(android.graphics.Bitmap in, android.graphics.Bitmap out);
}