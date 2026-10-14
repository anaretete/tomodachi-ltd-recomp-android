package com.recomp.tomodachiltd;

import android.app.AlertDialog;
import android.app.NativeActivity;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.graphics.Color;
import android.graphics.Typeface;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.ScrollView;
import android.widget.TextView;
import android.widget.Toast;

import androidx.documentfile.provider.DocumentFile;

import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;

public class TomodachiActivity extends NativeActivity {
    private static final String TAG = "TomodachiActivity";

    private static final int REQ_PROD_KEYS = 2001;
    private static final int REQ_GAME_NSP  = 2002;
    private static final int REQ_ROMFS_DIR = 2003;

    private static TomodachiActivity sInstance;
    private AlertDialog mInstallerDialog;

    private TextView mKeysStatusText;
    private TextView mGameStatusText;
    private Button mLaunchButton;
    private ProgressBar mProgressBar;
    private TextView mProgressLabel;

    public static native void nativeOnAssetsProvisioned();

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        sInstance = this;
        super.onCreate(savedInstanceState);
        Log.i(TAG, "TomodachiActivity initialized.");
    }

    @Override
    protected void onDestroy() {
        if (sInstance == this) {
            sInstance = null;
        }
        super.onDestroy();
    }

    public static File getKeysDir(Context context) {
        File dir = new File(context.getFilesDir(), "keys");
        if (!dir.exists()) dir.mkdirs();
        return dir;
    }

    public static File getRomFsDir(Context context) {
        File externalFiles = context.getExternalFilesDir(null);
        File baseDir = (externalFiles != null) ? externalFiles : context.getFilesDir();
        File romfs = new File(baseDir, "romfs");
        if (!romfs.exists()) romfs.mkdirs();
        return romfs;
    }

    public static File getGamesDir(Context context) {
        File dir = new File(context.getFilesDir(), "games");
        if (!dir.exists()) dir.mkdirs();
        return dir;
    }

    public static boolean checkKeysExist(Context context) {
        File prodKeys = new File(getKeysDir(context), "prod.keys");
        return prodKeys.exists() && prodKeys.length() > 0;
    }

    public static boolean checkGameExists(Context context) {
        File romfs = getRomFsDir(context);
        if (romfs.exists() && romfs.isDirectory()) {
            String[] list = romfs.list();
            if (list != null && list.length > 0) return true;
        }
        File nsp = new File(getGamesDir(context), "tomodachi.nsp");
        return nsp.exists() && nsp.length() > 0;
    }

    public static void showInstallerInterface() {
        if (sInstance == null) return;
        sInstance.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                sInstance.displayInstallerDialog();
            }
        });
    }

    private void displayInstallerDialog() {
        if (mInstallerDialog != null && mInstallerDialog.isShowing()) {
            updateInstallerStatus();
            return;
        }

        ScrollView scrollView = new ScrollView(this);
        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setPadding(dp(24), dp(20), dp(24), dp(20));
        root.setBackgroundColor(Color.parseColor("#121820"));

        // Header Title
        TextView title = new TextView(this);
        title.setText("Tomodachi Life Recomp");
        title.setTextColor(Color.parseColor("#5dade2"));
        title.setTextSize(TypedValue.COMPLEX_UNIT_SP, 20);
        title.setTypeface(null, Typeface.BOLD);
        title.setGravity(Gravity.CENTER);
        root.addView(title);

        TextView subtitle = new TextView(this);
        subtitle.setText("Native Android Setup & Asset Installer");
        subtitle.setTextColor(Color.parseColor("#aed6f1"));
        subtitle.setTextSize(TypedValue.COMPLEX_UNIT_SP, 13);
        subtitle.setGravity(Gravity.CENTER);
        subtitle.setPadding(0, dp(4), 0, dp(16));
        root.addView(subtitle);

        // --- SECTION 1: PROD.KEYS ---
        LinearLayout keysBox = createSectionCard("1. Nintendo Switch Keys (prod.keys)");
        mKeysStatusText = new TextView(this);
        mKeysStatusText.setTextSize(TypedValue.COMPLEX_UNIT_SP, 13);
        mKeysStatusText.setPadding(0, dp(4), 0, dp(10));
        keysBox.addView(mKeysStatusText);

        Button btnKeys = createStyledButton("Select prod.keys File", "#2980b9");
        btnKeys.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                pickFile(REQ_PROD_KEYS, "*/*");
            }
        });
        keysBox.addView(btnKeys);
        root.addView(keysBox);

        // --- SECTION 2: GAME DATA (.NSP OR ROMFS) ---
        LinearLayout gameBox = createSectionCard("2. Game Dump (.nsp or romfs)");
        mGameStatusText = new TextView(this);
        mGameStatusText.setTextSize(TypedValue.COMPLEX_UNIT_SP, 13);
        mGameStatusText.setPadding(0, dp(4), 0, dp(10));
        gameBox.addView(mGameStatusText);

        LinearLayout gameBtnRow = new LinearLayout(this);
        gameBtnRow.setOrientation(LinearLayout.HORIZONTAL);

        Button btnNsp = createStyledButton("Select .nsp File", "#27ae60");
        LinearLayout.LayoutParams nspParams = new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1.0f);
        nspParams.rightMargin = dp(6);
        btnNsp.setLayoutParams(nspParams);
        btnNsp.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                pickFile(REQ_GAME_NSP, "*/*");
            }
        });
        gameBtnRow.addView(btnNsp);

        Button btnFolder = createStyledButton("Select romfs Folder", "#16a085");
        LinearLayout.LayoutParams folderParams = new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1.0f);
        folderParams.leftMargin = dp(6);
        btnFolder.setLayoutParams(folderParams);
        btnFolder.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                pickFolder(REQ_ROMFS_DIR);
            }
        });
        gameBtnRow.addView(btnFolder);
        gameBox.addView(gameBtnRow);
        root.addView(gameBox);

        // --- SECTION 3: PROGRESS ---
        mProgressLabel = new TextView(this);
        mProgressLabel.setTextColor(Color.WHITE);
        mProgressLabel.setTextSize(TypedValue.COMPLEX_UNIT_SP, 12);
        mProgressLabel.setVisibility(View.GONE);
        mProgressLabel.setPadding(0, dp(10), 0, dp(4));
        root.addView(mProgressLabel);

        mProgressBar = new ProgressBar(this, null, android.R.attr.progressBarStyleHorizontal);
        mProgressBar.setIndeterminate(true);
        mProgressBar.setVisibility(View.GONE);
        root.addView(mProgressBar);

        // --- SECTION 4: LAUNCH BUTTON ---
        mLaunchButton = createStyledButton("Start Game", "#2ecc71");
        LinearLayout.LayoutParams launchParams = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, dp(48));
        launchParams.topMargin = dp(16);
        mLaunchButton.setLayoutParams(launchParams);
        mLaunchButton.setTextSize(TypedValue.COMPLEX_UNIT_SP, 16);
        mLaunchButton.setTypeface(null, Typeface.BOLD);
        mLaunchButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                if (mInstallerDialog != null) {
                    mInstallerDialog.dismiss();
                }
                nativeOnAssetsProvisioned();
            }
        });
        root.addView(mLaunchButton);

        scrollView.addView(root);

        mInstallerDialog = new AlertDialog.Builder(this)
            .setView(scrollView)
            .setCancelable(false)
            .create();
        mInstallerDialog.show();

        updateInstallerStatus();
    }

    private void updateInstallerStatus() {
        boolean hasKeys = checkKeysExist(this);
        boolean hasGame = checkGameExists(this);

        if (mKeysStatusText != null) {
            if (hasKeys) {
                File keysFile = new File(getKeysDir(this), "prod.keys");
                mKeysStatusText.setText("Installed: prod.keys (" + (keysFile.length() / 1024) + " KB)");
                mKeysStatusText.setTextColor(Color.parseColor("#2ecc71"));
            } else {
                mKeysStatusText.setText("Missing: prod.keys required for decryption");
                mKeysStatusText.setTextColor(Color.parseColor("#e74c3c"));
            }
        }

        if (mGameStatusText != null) {
            if (hasGame) {
                mGameStatusText.setText("Installed: Game assets ready in romfs");
                mGameStatusText.setTextColor(Color.parseColor("#2ecc71"));
            } else {
                mGameStatusText.setText("Missing: Select your dumped .nsp or extracted romfs");
                mGameStatusText.setTextColor(Color.parseColor("#e74c3c"));
            }
        }

        if (mLaunchButton != null) {
            boolean ready = hasKeys && hasGame;
            mLaunchButton.setEnabled(ready);
            mLaunchButton.setBackgroundColor(ready ? Color.parseColor("#27ae60") : Color.parseColor("#7f8c8d"));
            mLaunchButton.setText(ready ? "Start Tomodachi Life" : "Awaiting Keys & Game Data");
        }
    }

    private void pickFile(int reqCode, String mime) {
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.setType(mime);
        startActivityForResult(intent, reqCode);
    }

    private void pickFolder(int reqCode) {
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);
        startActivityForResult(intent, reqCode);
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (resultCode != RESULT_OK || data == null) return;

        Uri uri = data.getData();
        if (uri == null) return;

        if (requestCode == REQ_PROD_KEYS) {
            copyKeysAsync(uri);
        } else if (requestCode == REQ_GAME_NSP) {
            copyNspAsync(uri);
        } else if (requestCode == REQ_ROMFS_DIR) {
            copyRomFsTreeAsync(uri);
        }
    }

    private void copyKeysAsync(final Uri uri) {
        showProgress("Copying prod.keys...");
        new Thread(new Runnable() {
            @Override
            public void run() {
                try {
                    File dest = new File(getKeysDir(TomodachiActivity.this), "prod.keys");
                    copyStream(getContentResolver().openInputStream(uri), new FileOutputStream(dest));
                    postSuccess("prod.keys installed successfully!");
                } catch (Exception e) {
                    postError("Failed to import keys: " + e.getMessage());
                }
            }
        }).start();
    }

    private void copyNspAsync(final Uri uri) {
        showProgress("Copying game .nsp package...");
        new Thread(new Runnable() {
            @Override
            public void run() {
                try {
                    File dest = new File(getGamesDir(TomodachiActivity.this), "tomodachi.nsp");
                    copyStream(getContentResolver().openInputStream(uri), new FileOutputStream(dest));
                    postSuccess("Game .nsp package installed!");
                } catch (Exception e) {
                    postError("Failed to import .nsp: " + e.getMessage());
                }
            }
        }).start();
    }

    private void copyRomFsTreeAsync(final Uri treeUri) {
        showProgress("Copying romfs directory structure...");
        new Thread(new Runnable() {
            @Override
            public void run() {
                try {
                    DocumentFile rootDir = DocumentFile.fromTreeUri(TomodachiActivity.this, treeUri);
                    File targetRomfs = getRomFsDir(TomodachiActivity.this);
                    if (rootDir != null) {
                        copyDocTree(rootDir, targetRomfs);
                    }
                    postSuccess("RomFS directory installed successfully!");
                } catch (Exception e) {
                    postError("Failed to copy romfs: " + e.getMessage());
                }
            }
        }).start();
    }

    private void copyDocTree(DocumentFile src, File destDir) throws Exception {
        if (!destDir.exists()) destDir.mkdirs();
        for (DocumentFile file : src.listFiles()) {
            if (file.isDirectory()) {
                copyDocTree(file, new File(destDir, file.getName()));
            } else {
                File target = new File(destDir, file.getName());
                InputStream in = getContentResolver().openInputStream(file.getUri());
                OutputStream out = new FileOutputStream(target);
                copyStream(in, out);
            }
        }
    }

    private void copyStream(InputStream in, OutputStream out) throws Exception {
        if (in == null || out == null) return;
        byte[] buffer = new byte[65536];
        int read;
        while ((read = in.read(buffer)) != -1) {
            out.write(buffer, 0, read);
        }
        in.close();
        out.flush();
        out.close();
    }

    private void showProgress(final String label) {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                if (mProgressLabel != null) {
                    mProgressLabel.setText(label);
                    mProgressLabel.setVisibility(View.VISIBLE);
                }
                if (mProgressBar != null) {
                    mProgressBar.setVisibility(View.VISIBLE);
                }
            }
        });
    }

    private void postSuccess(final String msg) {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                if (mProgressBar != null) mProgressBar.setVisibility(View.GONE);
                if (mProgressLabel != null) mProgressLabel.setVisibility(View.GONE);
                Toast.makeText(TomodachiActivity.this, msg, Toast.LENGTH_SHORT).show();
                updateInstallerStatus();
            }
        });
    }

    private void postError(final String error) {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                if (mProgressBar != null) mProgressBar.setVisibility(View.GONE);
                if (mProgressLabel != null) mProgressLabel.setVisibility(View.GONE);
                Toast.makeText(TomodachiActivity.this, error, Toast.LENGTH_LONG).show();
                updateInstallerStatus();
            }
        });
    }

    private LinearLayout createSectionCard(String titleText) {
        LinearLayout card = new LinearLayout(this);
        card.setOrientation(LinearLayout.VERTICAL);
        card.setBackgroundColor(Color.parseColor("#1c2430"));
        card.setPadding(dp(14), dp(12), dp(14), dp(12));
        LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        params.bottomMargin = dp(14);
        card.setLayoutParams(params);

        TextView label = new TextView(this);
        label.setText(titleText);
        label.setTextColor(Color.WHITE);
        label.setTextSize(TypedValue.COMPLEX_UNIT_SP, 14);
        label.setTypeface(null, Typeface.BOLD);
        card.addView(label);

        return card;
    }

    private Button createStyledButton(String text, String colorHex) {
        Button btn = new Button(this);
        btn.setText(text);
        btn.setTextColor(Color.WHITE);
        btn.setBackgroundColor(Color.parseColor(colorHex));
        btn.setTextSize(TypedValue.COMPLEX_UNIT_SP, 13);
        btn.setPadding(dp(8), dp(8), dp(8), dp(8));
        return btn;
    }

    private int dp(int value) {
        return (int) TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, value, getResources().getDisplayMetrics());
    }

    public static void showModalDialog(final String title, final String message) {
        if (sInstance == null) return;
        sInstance.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                try {
                    new AlertDialog.Builder(sInstance)
                        .setTitle(title)
                        .setMessage(message)
                        .setPositiveButton("OK", null)
                        .setCancelable(false)
                        .show();
                } catch (Exception e) {
                    Log.e(TAG, "Error displaying dialog", e);
                }
            }
        });
    }
}
