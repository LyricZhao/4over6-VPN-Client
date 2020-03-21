package com.lyricz.a4over6vpn;

import android.Manifest;
import android.content.Context;
import android.content.pm.PackageManager;
import android.os.Environment;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import androidx.appcompat.app.AppCompatActivity;
import android.os.Bundle;
import android.widget.TextView;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;

import java.io.BufferedInputStream;
import java.io.File;
import java.io.FileInputStream;
import java.util.ArrayList;
import java.util.List;
import java.util.Timer;
import java.util.TimerTask;

// A pipe implement only for reading
class Pipe {
    // Singleton
    static int BUFFER_LENGTH = 4096;
    static String TAG = "Java.Pipe";

    static int pipe_count = 0;
    static File directory;

    // Object Oriented
    private File file;
    private byte[] buffer;

    Pipe() {
        pipe_count += 1;
        int id = pipe_count;
        file = new File(directory, "pipe_" + String.valueOf(id));
        buffer = new byte[BUFFER_LENGTH];
    }

    public String path() {
        return file.getAbsolutePath();
    }

    public void clean() {
        if (file.exists()) {
            file.delete();
        }
    }

    public String read() {
        int length = 0;
        try {
            FileInputStream stream = new FileInputStream(file);
            length = (new BufferedInputStream(stream)).read(buffer);
        } catch (Exception exception) {
            Log.e(TAG, "Error while reading pipe " + file.getName());
        }
        return length > 0 ? new String(buffer) : "";
    }
}

public class MainActivity extends AppCompatActivity {

    // Used to load the 'native-lib' library on application startup.
    static {
        System.loadLibrary("native-lib");
    }

    // UI handler
    private static Handler ui = new Handler(Looper.getMainLooper());

    // Parameters
    static int TIMER_INTERVAL = 1000;
    static String TAG = "MainActivity";

    // UI components
    private TextView statisticsView;

    // Variables
    private boolean running;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        running = false;

        // Link cache directory
        Pipe.directory = getApplicationContext().getCacheDir();

        // Link UI components
        linkUI();

        // For data statistics
        startStatisticsTimer();
    }

    protected void linkUI() {
        statisticsView = findViewById(R.id.statistics);
    }

    protected void startStatisticsTimer() {
        Timer timer = new Timer();
        TimerTask task = new TimerTask() {
            @Override
            public void run() {
                if (!running) {
                    cancel();
                }
                String info = getBackendStatistics();

                // Update UI
                Runnable update = new Runnable() {
                    @Override
                    public void run() {
                        statisticsView.setText(info);
                    }
                };
                ui.post(update);
            }
        };

        timer.schedule(task, 0, TIMER_INTERVAL);
    }

    /**
     * A native method that is implemented by the 'native-lib' native library,
     * which is packaged with this application.
     */
    public native String getBackendStatistics();

    public native void createBackendTunnel();

    public native void terminateBackendTunnel();
}
