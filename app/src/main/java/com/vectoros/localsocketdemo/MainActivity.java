package com.vectoros.localsocketdemo;

import androidx.appcompat.app.AppCompatActivity;

import android.os.Bundle;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;

public class MainActivity extends AppCompatActivity {

    // Used to load the 'native-lib' library on application startup.
    static {
        System.loadLibrary("native-lib");
    }
    private Button start, stop, quit;

    private View.OnClickListener clickListener = new View.OnClickListener() {
        @Override
        public void onClick(View v) {
            switch (v.getId()){
                case R.id.start:
                    serverCommand(1);
                    break;
                case R.id.stop:
                    serverCommand(2);
                    break;
                case R.id.quit:
                    serverCommand(0);
                    break;
            }
        }
    };

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        // Example of a call to a native method
        start = findViewById(R.id.start);
        stop = findViewById(R.id.stop);
        quit = findViewById(R.id.quit);
        start.setOnClickListener(clickListener);
        stop.setOnClickListener(clickListener);
        quit.setOnClickListener(clickListener);
    }

    @Override
    protected void onResume() {
        super.onResume();
        startServer();
    }

    @Override
    protected void onPause() {
        super.onPause();
        stopServer();
    }

    /**
     * A native method that is implemented by the 'native-lib' native library,
     * which is packaged with this application.
     */
    public native String stringFromJNI();

    public native void startServer();

    public native void stopServer();

    public native void connectToServer();

    public native void serverCommand(int cmd);

    public native void responseData(int msg);
}