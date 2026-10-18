package com.brruham.nes;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.text.InputType;
import android.view.View;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;
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
            if (r == 0) { NesCore.setInput(0); view.setLoaded(true); view.begin(); }
            else if (r == 1) Toast.makeText(this, "Bukan file .nes yang valid", Toast.LENGTH_LONG).show();
            else if (r == 2) Toast.makeText(this, "File ROM rusak / terpotong", Toast.LENGTH_LONG).show();
            else Toast.makeText(this, "Mapper " + (r - 100) + " belum didukung (baru 0,1,2,3,7)", Toast.LENGTH_LONG).show();
        } catch (Exception e) {
            Toast.makeText(this, "Gagal membaca file", Toast.LENGTH_LONG).show();
        }
    }

    @Override protected void onPause() { super.onPause(); view.pauseEmu(); }
    @Override protected void onResume() { super.onResume(); view.resumeEmu(); }

    // ---------- Cheat panel ----------
    private Button btn(Context cx, String t, View.OnClickListener l) {
        Button b = new Button(cx);
        b.setText(t); b.setTextSize(11); b.setMinWidth(0); b.setMinimumWidth(0);
        b.setPadding(8, 0, 8, 0); b.setOnClickListener(l);
        return b;
    }

    private void addW(LinearLayout r, View v, float w) {
        r.addView(v, new LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, w));
    }

    private LinearLayout row(Context cx) {
        LinearLayout r = new LinearLayout(cx);
        r.setOrientation(LinearLayout.HORIZONTAL);
        return r;
    }

    private int num(EditText e) {
        try { return Math.max(0, Math.min(255, Integer.parseInt(e.getText().toString().trim()))); }
        catch (Exception x) { return -1; }
    }

    private Button filt(final Context cx, String t, final int mode, final EditText val, final Runnable[] ref) {
        return btn(cx, t, x -> {
            int v = num(val);
            if (mode == 0 && v < 0) { Toast.makeText(cx, "Isi nilai dulu", Toast.LENGTH_SHORT).show(); return; }
            int r = NesCore.searchFilter(mode, Math.max(v, 0));
            if (r < 0) Toast.makeText(cx, "Tekan MULAI BARU dulu", Toast.LENGTH_SHORT).show();
            ref[0].run();
        });
    }

    void cheat() {
        AlertDialog.Builder b = new AlertDialog.Builder(this, android.R.style.Theme_DeviceDefault_Dialog_Alert);
        final Context cx = b.getContext();
        final LinearLayout root = new LinearLayout(cx);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setPadding(24, 8, 24, 8);
        ScrollView sv = new ScrollView(cx);
        sv.addView(root);

        TextView hint = new TextView(cx);
        hint.setTextSize(12);
        hint.setText("1) MULAI BARU  2) Tutup, main (ubah nilai di game, mis. nyawa berkurang)  3) Buka lagi, pilih filter (BERUBAH/TURUN/SAMA=nilai), ulangi sampai kandidat <= 20  4) FREEZE");
        final EditText val = new EditText(cx);
        val.setHint("Nilai 0-255");
        val.setInputType(InputType.TYPE_CLASS_NUMBER);
        val.setSingleLine(true);
        final TextView info = new TextView(cx);
        final LinearLayout list = new LinearLayout(cx);
        list.setOrientation(LinearLayout.VERTICAL);
        final Runnable[] ref = new Runnable[1];

        ref[0] = () -> {
            list.removeAllViews();
            int n = NesCore.searchCount();
            info.setText(n < 0 ? "Belum ada snapshot. Tekan MULAI BARU." : "Kandidat: " + n + (n > 20 ? " (filter lagi sampai <= 20)" : ""));
            for (int p : NesCore.freezeGet()) {
                final int a = p >> 8, cv = p & 0xFF;
                LinearLayout r = row(cx);
                TextView t = new TextView(cx);
                t.setText(String.format("BEKU $%03X = %d", a, cv));
                addW(r, t, 2f);
                addW(r, btn(cx, "LEPAS", x -> { NesCore.freeze(a, 0, false); ref[0].run(); }), 1f);
                list.addView(r);
            }
            if (n >= 0 && n <= 20) {
                for (int p : NesCore.searchGet(20)) {
                    final int a = p >> 8, cv = p & 0xFF;
                    LinearLayout r = row(cx);
                    TextView t = new TextView(cx);
                    t.setText(String.format("$%03X = %d", a, cv));
                    addW(r, t, 2f);
                    addW(r, btn(cx, "FREEZE", x -> {
                        int v = num(val); if (v < 0) v = cv;
                        NesCore.freeze(a, v, true); ref[0].run();
                    }), 1f);
                    addW(r, btn(cx, "POKE", x -> {
                        int v = num(val); if (v < 0) return;
                        NesCore.poke(a, v); ref[0].run();
                    }), 1f);
                    list.addView(r);
                }
            }
        };

        LinearLayout r1 = row(cx), r2 = row(cx);
        addW(r1, btn(cx, "MULAI BARU", x -> { NesCore.searchReset(); ref[0].run(); }), 1f);
        addW(r1, filt(cx, "SAMA=NILAI", 0, val, ref), 1f);
        addW(r2, filt(cx, "BERUBAH", 1, val, ref), 1f);
        addW(r2, filt(cx, "TETAP", 2, val, ref), 1f);
        addW(r2, filt(cx, "NAIK", 3, val, ref), 1f);
        addW(r2, filt(cx, "TURUN", 4, val, ref), 1f);

        root.addView(hint);
        root.addView(val);
        root.addView(r1);
        root.addView(r2);
        root.addView(info);
        root.addView(list);

        b.setView(sv);
        b.setNegativeButton("Tutup", null);
        ref[0].run();
        b.show();
    }
}
