package com.lyricz.a4over6vpn;

import android.Manifest;
import android.content.Context;
import android.content.pm.PackageManager;
import android.os.Environment;
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

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        checkPermissions();
        Pipe.directory = getApplicationContext().getCacheDir();
    }

    private void checkPermissions() {
        String[] permissions = new String[] {
            Manifest.permission.INTERNET,
            Manifest.permission.READ_EXTERNAL_STORAGE,
            Manifest.permission.WRITE_EXTERNAL_STORAGE,
            Manifest.permission.BIND_VPN_SERVICE
        };

        List<String> needed = new ArrayList<>();

        for (String permission: permissions) {
            int granted = ContextCompat.checkSelfPermission(this, permission);
            if (granted != PackageManager.PERMISSION_GRANTED) {
                needed.add(permission);
            }
        }

        if (!needed.isEmpty()) {
            ActivityCompat.requestPermissions(this, needed.toArray(new String[0]), 0);
        }
    }

    /**
     * A native method that is implemented by the 'native-lib' native library,
     * which is packaged with this application.
     */
    public native void createBackendTunnel();
    public native void terminateBackendTunnel();
}
