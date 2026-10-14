// ==============================================================================
// android/app/src/main/java/com/recomp/tomodachiltd/MainActivity.java
// Dedicated Setup, Asset Installer & Launcher Interface for Tomodachi Life Recomp
// ==============================================================================

package com.recomp.tomodachiltd;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.pm.FeatureInfo;
import android.content.pm.PackageManager;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.HorizontalScrollView;
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

public class MainActivity extends Activity {
    private static final String TAG = "TomodachiLauncher";

    private static final int REQ_KEYS   = 1001;
    private static final int REQ_NSP    = 1002;
    private static final int REQ_ROMFS  = 1003;

    private TextView mKeysStatusText;
    private TextView mGameStatusText;
    private Button mLaunchButton;
    private Button mTestModeButton;
    private ProgressBar mProgressBar;
    private TextView mProgressLabel;
    private TextView mDeviceSpecText;

    private final Handler mHandler = new Handler(Looper.getMainLooper());

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // Enable immersive full-screen landscape
        requestWindowFeature(Window.FEATURE_NO_TITLE);
        getWindow().setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN, WindowManager.LayoutParams.FLAG_FULLSCREEN);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            getWindow().getAttributes().layoutInDisplayCutoutMode =
                WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
        }

        buildUserInterface();
        refreshAssetStatus();
    }

    @Override
    protected void onResume() {
        super.onResume();
        hideSystemUI();
        refreshAssetStatus();
    }

    private void hideSystemUI() {
        View decorView = getWindow().getDecorView();
        decorView.setSystemUiVisibility(
            View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
            | View.SYSTEM_UI_FLAG_LAYOUT_STABLE
            | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
            | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
            | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
            | View.SYSTEM_UI_FLAG_FULLSCREEN
        );
    }

    public static File getKeysDir(Context context) {
        File dir = new File(context.getFilesDir(), "keys");
        if (!dir.exists()) dir.mkdirs();
        return dir;
    }

    public static File getRomFsDir(Context context) {
        File ext = context.getExternalFilesDir(null);
        File base = (ext != null) ? ext : context.getFilesDir();
        File romfs = new File(base, "romfs");
        if (!romfs.exists()) romfs.mkdirs();
        return romfs;
    }

    public static File getGamesDir(Context context) {
        File dir = new File(context.getFilesDir(), "games");
        if (!dir.exists()) dir.mkdirs();
        return dir;
    }

    public static boolean checkKeysExist(Context context) {
        File keysFile = new File(getKeysDir(context), "prod.keys");
        return keysFile.exists() && keysFile.length() > 0;
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

    private void buildUserInterface() {
        ScrollView scrollView = new ScrollView(this);
        scrollView.setFillViewport(true);
        scrollView.setBackgroundColor(Color.parseColor("#0F141C"));

        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setPadding(dp(20), dp(16), dp(20), dp(20));

        // 1. HEADER SECTION
        LinearLayout header = new LinearLayout(this);
        header.setOrientation(LinearLayout.VERTICAL);
        header.setGravity(Gravity.CENTER);

        TextView title = new TextView(this);
        title.setText("TOMODACHI LIFE: LIVING THE DREAM");
        title.setTextColor(Color.parseColor("#54A0FF"));
        title.setTextSize(TypedValue.COMPLEX_UNIT_SP, 22);
        title.setTypeface(Typeface.create("sans-serif-black", Typeface.BOLD));
        title.setGravity(Gravity.CENTER);
        header.addView(title);

        TextView subtitle = new TextView(this);
        subtitle.setText("Native Nintendo Switch Recompilation Harness • Android Edition");
        subtitle.setTextColor(Color.parseColor("#8395A7"));
        subtitle.setTextSize(TypedValue.COMPLEX_UNIT_SP, 12);
        subtitle.setGravity(Gravity.CENTER);
        subtitle.setPadding(0, dp(2), 0, dp(10));
        header.addView(subtitle);

        // Hardware Diagnostic Badge
        mDeviceSpecText = new TextView(this);
        mDeviceSpecText.setTextSize(TypedValue.COMPLEX_UNIT_SP, 11);
        mDeviceSpecText.setTextColor(Color.parseColor("#00D2D3"));
        mDeviceSpecText.setGravity(Gravity.CENTER);
        mDeviceSpecText.setPadding(dp(12), dp(4), dp(12), dp(4));
        mDeviceSpecText.setBackground(createRoundedBackground("#1B293C", "#2C3E50", 6));
        mDeviceSpecText.setText(getSystemDiagnostics());
        header.addView(mDeviceSpecText);

        root.addView(header);

        // 2. SETUP PANELS (HORIZONTAL ON TABLETS/LANDSCAPE, STACKED)
        LinearLayout panelsRow = new LinearLayout(this);
        panelsRow.setOrientation(LinearLayout.HORIZONTAL);
        panelsRow.setPadding(0, dp(14), 0, dp(10));

        // --- PANEL 1: PROD.KEYS ---
        LinearLayout keysCard = createPanelCard("1. Decryption Keys (prod.keys)");
        LinearLayout.LayoutParams cardParams1 = new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1.0f);
        cardParams1.rightMargin = dp(8);
        keysCard.setLayoutParams(cardParams1);

        mKeysStatusText = new TextView(this);
        mKeysStatusText.setTextSize(TypedValue.COMPLEX_UNIT_SP, 12);
        mKeysStatusText.setPadding(0, dp(2), 0, dp(10));
        keysCard.addView(mKeysStatusText);

        LinearLayout keysBtnRow = new LinearLayout(this);
        keysBtnRow.setOrientation(LinearLayout.HORIZONTAL);

        Button btnPickKeys = createStyledButton("Select prod.keys", "#2980B9");
        LinearLayout.LayoutParams btnP1 = new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1.0f);
        btnP1.rightMargin = dp(4);
        btnPickKeys.setLayoutParams(btnP1);
        btnPickKeys.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                pickDocument(REQ_KEYS, "*/*");
            }
        });
        keysBtnRow.addView(btnPickKeys);

        Button btnKeysInfo = createStyledButton("Info", "#34495E");
        LinearLayout.LayoutParams btnP2 = new LinearLayout.LayoutParams(dp(70), ViewGroup.LayoutParams.WRAP_CONTENT);
        btnKeysInfo.setLayoutParams(btnP2);
        btnKeysInfo.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                showInfoDialog(
                    "Switch Decryption Keys",
                    "The recompilation engine requires your console's 'prod.keys' file to decrypt game audio and asset packages.\n\n"
                    + "Path: " + getKeysDir(MainActivity.this).getAbsolutePath() + "/prod.keys\n\n"
                    + "Tap 'Select prod.keys' to browse and import your keys file directly."
                );
            }
        });
        keysBtnRow.addView(btnKeysInfo);
        keysCard.addView(keysBtnRow);
        panelsRow.addView(keysCard);

        // --- PANEL 2: GAME DATA ---
        LinearLayout gameCard = createPanelCard("2. Game Dump (.nsp or romfs)");
        LinearLayout.LayoutParams cardParams2 = new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1.0f);
        cardParams2.leftMargin = dp(8);
        gameCard.setLayoutParams(cardParams2);

        mGameStatusText = new TextView(this);
        mGameStatusText.setTextSize(TypedValue.COMPLEX_UNIT_SP, 12);
        mGameStatusText.setPadding(0, dp(2), 0, dp(10));
        gameCard.addView(mGameStatusText);

        LinearLayout gameBtnRow = new LinearLayout(this);
        gameBtnRow.setOrientation(LinearLayout.HORIZONTAL);

        Button btnPickNsp = createStyledButton("Import .nsp", "#27AE60");
        LinearLayout.LayoutParams btnG1 = new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1.0f);
        btnG1.rightMargin = dp(4);
        btnPickNsp.setLayoutParams(btnG1);
        btnPickNsp.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                pickDocument(REQ_NSP, "*/*");
            }
        });
        gameBtnRow.addView(btnPickNsp);

        Button btnPickRomfs = createStyledButton("Import RomFS", "#16A085");
        LinearLayout.LayoutParams btnG2 = new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1.0f);
        btnG2.rightMargin = dp(4);
        btnPickRomfs.setLayoutParams(btnG2);
        btnPickRomfs.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                pickFolder(REQ_ROMFS);
            }
        });
        gameBtnRow.addView(btnPickRomfs);

        Button btnGameInfo = createStyledButton("Paths", "#34495E");
        LinearLayout.LayoutParams btnG3 = new LinearLayout.LayoutParams(dp(70), ViewGroup.LayoutParams.WRAP_CONTENT);
        btnGameInfo.setLayoutParams(btnG3);
        btnGameInfo.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                showInfoDialog(
                    "Game Asset Locations",
                    "You can install game files using the buttons above or manually copy them using any Android file manager:\n\n"
                    + "• RomFS Directory:\n" + getRomFsDir(MainActivity.this).getAbsolutePath() + "\n\n"
                    + "• NSP Package File:\n" + getGamesDir(MainActivity.this).getAbsolutePath() + "/tomodachi.nsp"
                );
            }
        });
        gameBtnRow.addView(btnGameInfo);
        gameCard.addView(gameBtnRow);
        panelsRow.addView(gameCard);

        root.addView(panelsRow);

        // 3. PROGRESS SECTION
        mProgressLabel = new TextView(this);
        mProgressLabel.setTextColor(Color.WHITE);
        mProgressLabel.setTextSize(TypedValue.COMPLEX_UNIT_SP, 12);
        mProgressLabel.setVisibility(View.GONE);
        mProgressLabel.setPadding(0, dp(4), 0, dp(2));
        root.addView(mProgressLabel);

        mProgressBar = new ProgressBar(this, null, android.R.attr.progressBarStyleHorizontal);
        mProgressBar.setIndeterminate(true);
        mProgressBar.setVisibility(View.GONE);
        root.addView(mProgressBar);

        // 4. ACTION CONTROLS SECTION
        LinearLayout actionRow = new LinearLayout(this);
        actionRow.setOrientation(LinearLayout.HORIZONTAL);
        actionRow.setPadding(0, dp(14), 0, 0);

        // Primary Launch Button
        mLaunchButton = createStyledButton("🚀 START TOMODACHI LIFE", "#2ECC71");
        LinearLayout.LayoutParams launchParams = new LinearLayout.LayoutParams(0, dp(50), 1.5f);
        launchParams.rightMargin = dp(8);
        mLaunchButton.setLayoutParams(launchParams);
        mLaunchButton.setTextSize(TypedValue.COMPLEX_UNIT_SP, 15);
        mLaunchButton.setTypeface(Typeface.create("sans-serif-medium", Typeface.BOLD));
        mLaunchButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                if (checkKeysExist(MainActivity.this) && checkGameExists(MainActivity.this)) {
                    launchGameEngine();
                } else {
                    showMissingAssetsAlert();
                }
            }
        });
        actionRow.addView(mLaunchButton);

        // Demo / Viewport & Controller Test Mode Button
        mTestModeButton = createStyledButton("🎮 Viewport & Controls Test", "#8E44AD");
        LinearLayout.LayoutParams testParams = new LinearLayout.LayoutParams(0, dp(50), 1.0f);
        testParams.rightMargin = dp(8);
        mTestModeButton.setLayoutParams(testParams);
        mTestModeButton.setTextSize(TypedValue.COMPLEX_UNIT_SP, 13);
        mTestModeButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                Toast.makeText(MainActivity.this, "Launching Vulkan Viewport & Virtual Touchpad Test...", Toast.LENGTH_SHORT).show();
                launchGameEngine();
            }
        });
        actionRow.addView(mTestModeButton);

        // Refresh Button
        Button btnRefresh = createStyledButton("🔄 Refresh", "#34495E");
        LinearLayout.LayoutParams refParams = new LinearLayout.LayoutParams(dp(100), dp(50));
        btnRefresh.setLayoutParams(refParams);
        btnRefresh.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                refreshAssetStatus();
                Toast.makeText(MainActivity.this, "Asset status updated.", Toast.LENGTH_SHORT).show();
            }
        });
        actionRow.addView(btnRefresh);

        root.addView(actionRow);

        scrollView.addView(root);
        setContentView(scrollView);
    }

    private void refreshAssetStatus() {
        boolean hasKeys = checkKeysExist(this);
        boolean hasGame = checkGameExists(this);

        if (mKeysStatusText != null) {
            if (hasKeys) {
                File keysFile = new File(getKeysDir(this), "prod.keys");
                long kb = keysFile.length() / 1024;
                mKeysStatusText.setText("✅ Installed: prod.keys (" + kb + " KB ready)");
                mKeysStatusText.setTextColor(Color.parseColor("#2ECC71"));
            } else {
                mKeysStatusText.setText("❌ Missing: prod.keys (Required for decryption)");
                mKeysStatusText.setTextColor(Color.parseColor("#E74C3C"));
            }
        }

        if (mGameStatusText != null) {
            if (hasGame) {
                File romfs = getRomFsDir(this);
                File nsp = new File(getGamesDir(this), "tomodachi.nsp");
                if (nsp.exists() && nsp.length() > 0) {
                    long mb = nsp.length() / (1024 * 1024);
                    mGameStatusText.setText("✅ Installed: tomodachi.nsp (" + mb + " MB ready)");
                } else {
                    mGameStatusText.setText("✅ Installed: RomFS asset directory ready");
                }
                mGameStatusText.setTextColor(Color.parseColor("#2ECC71"));
            } else {
                mGameStatusText.setText("❌ Missing: Install .nsp or romfs directory");
                mGameStatusText.setTextColor(Color.parseColor("#E74C3C"));
            }
        }

        if (mLaunchButton != null) {
            boolean ready = hasKeys && hasGame;
            mLaunchButton.setEnabled(true); // Always clickable so it gives feedback
            if (ready) {
                mLaunchButton.setText("🚀 START TOMODACHI LIFE");
                mLaunchButton.setBackground(createRoundedBackground("#27AE60", "#2ECC71", 8));
            } else {
                mLaunchButton.setText("⚠️ Complete Setup to Play");
                mLaunchButton.setBackground(createRoundedBackground("#7F8C8D", "#95A5A6", 8));
            }
        }
    }

    private void launchGameEngine() {
        Intent intent = new Intent(this, GameActivity.class);
        startActivity(intent);
    }

    private void showMissingAssetsAlert() {
        new AlertDialog.Builder(this)
            .setTitle("Asset Setup Incomplete")
            .setMessage(
                "Tomodachi Life Recomp requires your dumped Nintendo Switch game files and decryption keys:\n\n"
                + (!checkKeysExist(this) ? "• prod.keys is missing. Tap 'Select prod.keys' to import.\n" : "• prod.keys is ready.\n")
                + (!checkGameExists(this) ? "• Game data is missing. Tap 'Import .nsp' or 'Import RomFS'.\n\n" : "• Game data is ready.\n\n")
                + "Would you like to launch in Test Mode anyway to verify the Vulkan viewport and touch controls?"
            )
            .setPositiveButton("Launch Test Mode", new DialogInterface.OnClickListener() {
                @Override
                public void onClick(DialogInterface dialog, int which) {
                    launchGameEngine();
                }
            })
            .setNegativeButton("Return to Setup", null)
            .show();
    }

    private void pickDocument(int reqCode, String mime) {
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

        if (requestCode == REQ_KEYS) {
            importKeysAsync(uri);
        } else if (requestCode == REQ_NSP) {
            importNspAsync(uri);
        } else if (requestCode == REQ_ROMFS) {
            importRomfsAsync(uri);
        }
    }

    private void importKeysAsync(final Uri uri) {
        showProgress("Importing prod.keys...");
        new Thread(new Runnable() {
            @Override
            public void run() {
                try {
                    File dest = new File(getKeysDir(MainActivity.this), "prod.keys");
                    copyStream(getContentResolver().openInputStream(uri), new FileOutputStream(dest));
                    postSuccess("prod.keys imported successfully (" + (dest.length() / 1024) + " KB)!");
                } catch (Exception e) {
                    postError("Failed to import keys: " + e.getMessage());
                }
            }
        }).start();
    }

    private void importNspAsync(final Uri uri) {
        showProgress("Importing game .nsp package...");
        new Thread(new Runnable() {
            @Override
            public void run() {
                try {
                    File dest = new File(getGamesDir(MainActivity.this), "tomodachi.nsp");
                    copyStream(getContentResolver().openInputStream(uri), new FileOutputStream(dest));
                    long mb = dest.length() / (1024 * 1024);
                    postSuccess("Game .nsp package imported (" + mb + " MB)!");
                } catch (Exception e) {
                    postError("Failed to import .nsp: " + e.getMessage());
                }
            }
        }).start();
    }

    private void importRomfsAsync(final Uri treeUri) {
        showProgress("Copying RomFS folder structure...");
        new Thread(new Runnable() {
            @Override
            public void run() {
                try {
                    DocumentFile rootDir = DocumentFile.fromTreeUri(MainActivity.this, treeUri);
                    File targetRomfs = getRomFsDir(MainActivity.this);
                    if (rootDir != null) {
                        copyDocTree(rootDir, targetRomfs);
                    }
                    postSuccess("RomFS directory tree copied successfully!");
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
        mHandler.post(new Runnable() {
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
        mHandler.post(new Runnable() {
            @Override
            public void run() {
                if (mProgressBar != null) mProgressBar.setVisibility(View.GONE);
                if (mProgressLabel != null) mProgressLabel.setVisibility(View.GONE);
                Toast.makeText(MainActivity.this, msg, Toast.LENGTH_SHORT).show();
                refreshAssetStatus();
            }
        });
    }

    private void postError(final String error) {
        mHandler.post(new Runnable() {
            @Override
            public void run() {
                if (mProgressBar != null) mProgressBar.setVisibility(View.GONE);
                if (mProgressLabel != null) mProgressLabel.setVisibility(View.GONE);
                Toast.makeText(MainActivity.this, error, Toast.LENGTH_LONG).show();
                refreshAssetStatus();
            }
        });
    }

    private String getSystemDiagnostics() {
        String abi = (Build.SUPPORTED_ABIS != null && Build.SUPPORTED_ABIS.length > 0)
            ? Build.SUPPORTED_ABIS[0] : "arm64-v8a";

        boolean hasVulkan = getPackageManager().hasSystemFeature(PackageManager.FEATURE_VULKAN_HARDWARE_VERSION);
        int vkVer = 0;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
            for (FeatureInfo f : getPackageManager().getSystemAvailableFeatures()) {
                if (PackageManager.FEATURE_VULKAN_HARDWARE_VERSION.equals(f.name)) {
                    vkVer = f.version;
                    break;
                }
            }
        }

        String vkString = hasVulkan ? ("Vulkan Supported (v" + (vkVer >> 22) + "." + ((vkVer >> 12) & 0x3ff) + ")") : "Vulkan Fallback (EGL/GLES3)";
        return "ABI: " + abi + "  •  Graphics: " + vkString + "  •  Android " + Build.VERSION.RELEASE;
    }

    private LinearLayout createPanelCard(String headerTitle) {
        LinearLayout card = new LinearLayout(this);
        card.setOrientation(LinearLayout.VERTICAL);
        card.setPadding(dp(14), dp(12), dp(14), dp(12));
        card.setBackground(createRoundedBackground("#161E2E", "#27384E", 10));

        TextView label = new TextView(this);
        label.setText(headerTitle);
        label.setTextColor(Color.parseColor("#F0F6FC"));
        label.setTextSize(TypedValue.COMPLEX_UNIT_SP, 13);
        label.setTypeface(Typeface.create("sans-serif-medium", Typeface.BOLD));
        label.setPadding(0, 0, 0, dp(4));
        card.addView(label);

        return card;
    }

    private Button createStyledButton(String text, String colorHex) {
        Button btn = new Button(this);
        btn.setText(text);
        btn.setTextColor(Color.WHITE);
        btn.setTextSize(TypedValue.COMPLEX_UNIT_SP, 12);
        btn.setBackground(createRoundedBackground(colorHex, lightenColor(colorHex), 8));
        return btn;
    }

    private GradientDrawable createRoundedBackground(String fillColor, String strokeColor, int radiusDp) {
        GradientDrawable gd = new GradientDrawable();
        gd.setColor(Color.parseColor(fillColor));
        gd.setStroke(dp(1), Color.parseColor(strokeColor));
        gd.setCornerRadius(dp(radiusDp));
        return gd;
    }

    private String lightenColor(String hex) {
        return hex;
    }

    private void showInfoDialog(String title, String message) {
        new AlertDialog.Builder(this)
            .setTitle(title)
            .setMessage(message)
            .setPositiveButton("OK", null)
            .show();
    }

    private int dp(int value) {
        return (int) TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, value, getResources().getDisplayMetrics());
    }
}
