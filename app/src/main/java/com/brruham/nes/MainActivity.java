package com.brruham.nes;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.view.WindowManager;
import android.widget.Toast;
import java.io.ByteArrayOutputStream;
import java.io.InputStream;

public class MainActivity extends Activity {
    private GameView view;

    @Override protected void onCreate(Bundle b) {
        super.onCreate(b);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        view = new GameView(this);
        setContentView(view);
        pick();
    }

    void pick() {
        Intent i = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        i.addCategory(Intent.CATEGORY_OPENABLE);
        i.setType("*/*");
        startActivityForResult(i, 1);
    }

    @Override protected void onActivityResult(int rq, int rs, Intent d) {
        if (rq != 1 || rs != RESULT_OK || d == null) return;
        try {
            InputStream in = getContentResolver().openInputStream(d.getData());
            ByteArrayOutputStream bo = new ByteArrayOutputStream();
            byte[] buf = new byte[65536];
            int n;
            while ((n = in.read(buf)) > 0) bo.write(buf, 0, n);
            in.close();
            view.halt();
            int r = NesCore.load(bo.toByteArray());
            if (r == 0) { NesCore.setInput(0); view.setLoaded(true); }
            else if (r == 1) Toast.makeText(this, "Bukan file .nes yang valid", Toast.LENGTH_LONG).show();
            else if (r == 2) Toast.makeText(this, "File ROM rusak / terpotong", Toast.LENGTH_LONG).show();
            else Toast.makeText(this, "Mapper " + (r - 100) + " belum didukung (baru 0,1,2,3,7)", Toast.LENGTH_LONG).show();
        } catch (Exception e) {
            Toast.makeText(this, "Gagal membaca file", Toast.LENGTH_LONG).show();
        }
    }

    @Override protected void onPause() { super.onPause(); view.pauseEmu(); }
    @Override protected void onResume() { super.onResume(); view.resumeEmu(); }
}
