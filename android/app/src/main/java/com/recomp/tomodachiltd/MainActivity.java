// ==============================================================================
// android/app/src/main/java/com/recomp/tomodachiltd/MainActivity.java
// Advanced Switch Game Launcher, XCI & NSP Container Manager & Firmware Installer
// ==============================================================================

package com.recomp.tomodachiltd;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.pm.FeatureInfo;
import android.content.pm.PackageManager;
import android.content.res.AssetFileDescriptor;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.media.MediaPlayer;
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
import android.widget.ImageView;
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

    private static final int REQ_KEYS       = 1001;
    private static final int REQ_TITLE_KEYS = 1002;
    private static final int REQ_GAME_PKG   = 1003;
    private static final int REQ_ROMFS_DIR  = 1004;
    private static final int REQ_FIRMWARE   = 1005;

    private TextView mKeysStatusText;
    private TextView mFirmwareStatusText;
    private TextView mGameStatusText;
    private Button mLaunchButton;
    private Button mTestModeButton;
    private ProgressBar mProgressBar;
    private TextView mProgressLabel;
    private TextView mDeviceSpecText;

    // Game Library Card Views
    private LinearLayout mGameCardContainer;
    private ImageView mGameIconView;
    private TextView mGameTitleText;
    private TextView mGameMetaText;
    private TextView mGameBadgeText;

    private final Handler mHandler = new Handler(Looper.getMainLooper());
    private MediaPlayer mMediaPlayer;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        requestWindowFeature(Window.FEATURE_NO_TITLE);
        getWindow().setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN, WindowManager.LayoutParams.FLAG_FULLSCREEN);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            getWindow().getAttributes().layoutInDisplayCutoutMode =
                WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
        }

        buildUserInterface();
        refreshAllStatus();
    }

    @Override
    protected void onResume() {
        super.onResume();
        hideSystemUI();
        refreshAllStatus();
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

    public static File getFirmwareDir(Context context) {
        File dir = new File(context.getFilesDir(), "firmware/contents/registered");
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

    public static boolean checkFirmwareExist(Context context) {
        File fwDir = getFirmwareDir(context);
        if (fwDir.exists() && fwDir.isDirectory()) {
            String[] list = fwDir.list();
            return list != null && list.length > 5;
        }
        return false;
    }

    public static boolean checkGameExists(Context context) {
        File romfs = getRomFsDir(context);
        if (romfs.exists() && romfs.isDirectory()) {
            String[] list = romfs.list();
            if (list != null && list.length > 0) return true;
        }
        File gamesDir = getGamesDir(context);
        File xci = new File(gamesDir, "tomodachi_game.xci");
        File nsp = new File(gamesDir, "tomodachi_game.nsp");
        File oldNsp = new File(gamesDir, "tomodachi.nsp");
        return (xci.exists() && xci.length() > 0) || (nsp.exists() && nsp.length() > 0) || (oldNsp.exists() && oldNsp.length() > 0);
    }

    private void playSound(String soundAsset) {
        try {
            if (mMediaPlayer != null) {
                mMediaPlayer.release();
                mMediaPlayer = null;
            }
            AssetFileDescriptor afd = getAssets().openFd("sounds/" + soundAsset);
            mMediaPlayer = new MediaPlayer();
            mMediaPlayer.setDataSource(afd.getFileDescriptor(), afd.getStartOffset(), afd.getLength());
            afd.close();
            mMediaPlayer.prepare();
            mMediaPlayer.start();
        } catch (Exception ignored) {
        }
    }

    private void buildUserInterface() {
        ScrollView scrollView = new ScrollView(this);
        scrollView.setFillViewport(true);
        scrollView.setBackgroundColor(Color.parseColor("#0D1117"));

        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setPadding(dp(20), dp(14), dp(20), dp(20));

        // 1. HEADER SECTION
        LinearLayout header = new LinearLayout(this);
        header.setOrientation(LinearLayout.VERTICAL);
        header.setGravity(Gravity.CENTER);

        TextView title = new TextView(this);
        title.setText("TOMODACHI LIFE: LIVING THE DREAM");
        title.setTextColor(Color.parseColor("#58A6FF"));
        title.setTextSize(TypedValue.COMPLEX_UNIT_SP, 21);
        title.setTypeface(Typeface.create("sans-serif-black", Typeface.BOLD));
        title.setGravity(Gravity.CENTER);
        header.addView(title);

        TextView subtitle = new TextView(this);
        subtitle.setText("Native Nintendo Switch Recompilation Engine • Multi-Format Harness");
        subtitle.setTextColor(Color.parseColor("#8B949E"));
        subtitle.setTextSize(TypedValue.COMPLEX_UNIT_SP, 12);
        subtitle.setGravity(Gravity.CENTER);
        subtitle.setPadding(0, dp(2), 0, dp(8));
        header.addView(subtitle);

        // Hardware & Architecture Spec Pill
        mDeviceSpecText = new TextView(this);
        mDeviceSpecText.setTextSize(TypedValue.COMPLEX_UNIT_SP, 11);
        mDeviceSpecText.setTextColor(Color.parseColor("#39D353"));
        mDeviceSpecText.setGravity(Gravity.CENTER);
        mDeviceSpecText.setPadding(dp(12), dp(4), dp(12), dp(4));
        mDeviceSpecText.setBackground(createRoundedBackground("#161B22", "#30363D", 6));
        mDeviceSpecText.setText(getSystemDiagnostics());
        header.addView(mDeviceSpecText);

        root.addView(header);

        // 2. ACTIVE GAME CARD (HIGHLIGHTS LOADED XCI / NSP / ROMFS)
        mGameCardContainer = createGameCard();
        root.addView(mGameCardContainer);

        // 3. CONFIGURATION & PROVISIONING PANELS (3 HORIZONTAL CARDS)
        LinearLayout panelsRow = new LinearLayout(this);
        panelsRow.setOrientation(LinearLayout.HORIZONTAL);
        panelsRow.setPadding(0, dp(10), 0, dp(10));

        // --- CARD 1: KEYS ---
        LinearLayout keysCard = createPanelCard("1. Decryption Keys");
        LinearLayout.LayoutParams p1 = new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1.0f);
        p1.rightMargin = dp(6);
        keysCard.setLayoutParams(p1);

        mKeysStatusText = new TextView(this);
        mKeysStatusText.setTextSize(TypedValue.COMPLEX_UNIT_SP, 11);
        mKeysStatusText.setPadding(0, dp(2), 0, dp(8));
        keysCard.addView(mKeysStatusText);

        LinearLayout keysRow = new LinearLayout(this);
        keysRow.setOrientation(LinearLayout.HORIZONTAL);

        Button btnKeys = createStyledButton("prod.keys", "#1F6FEB");
        LinearLayout.LayoutParams k1 = new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1.0f);
        k1.rightMargin = dp(4);
        btnKeys.setLayoutParams(k1);
        btnKeys.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                playSound("button_click.wav");
                pickDocument(REQ_KEYS, "*/*");
            }
        });
        keysRow.addView(btnKeys);

        Button btnTitleKeys = createStyledButton("title.keys", "#238636");
        LinearLayout.LayoutParams k2 = new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1.0f);
        btnTitleKeys.setLayoutParams(k2);
        btnTitleKeys.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                playSound("button_click.wav");
                pickDocument(REQ_TITLE_KEYS, "*/*");
            }
        });
        keysRow.addView(btnTitleKeys);
        keysCard.addView(keysRow);
        panelsRow.addView(keysCard);

        // --- CARD 2: GAME PACKAGE (.XCI / .NSP) ---
        LinearLayout gameCard = createPanelCard("2. Game Card / Package");
        LinearLayout.LayoutParams p2 = new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1.2f);
        p2.rightMargin = dp(6);
        p2.leftMargin = dp(6);
        gameCard.setLayoutParams(p2);

        mGameStatusText = new TextView(this);
        mGameStatusText.setTextSize(TypedValue.COMPLEX_UNIT_SP, 11);
        mGameStatusText.setPadding(0, dp(2), 0, dp(8));
        gameCard.addView(mGameStatusText);

        LinearLayout gameRow = new LinearLayout(this);
        gameRow.setOrientation(LinearLayout.HORIZONTAL);

        Button btnGamePkg = createStyledButton("Install .XCI / .NSP", "#238636");
        LinearLayout.LayoutParams g1 = new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1.3f);
        g1.rightMargin = dp(4);
        btnGamePkg.setLayoutParams(g1);
        btnGamePkg.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                playSound("button_click.wav");
                pickDocument(REQ_GAME_PKG, "*/*");
            }
        });
        gameRow.addView(btnGamePkg);

        Button btnRomfs = createStyledButton("RomFS Folder", "#161B22");
        LinearLayout.LayoutParams g2 = new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1.0f);
        btnRomfs.setLayoutParams(g2);
        btnRomfs.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                playSound("button_click.wav");
                pickFolder(REQ_ROMFS_DIR);
            }
        });
        gameRow.addView(btnRomfs);
        gameCard.addView(gameRow);
        panelsRow.addView(gameCard);

        // --- CARD 3: FIRMWARE ---
        LinearLayout fwCard = createPanelCard("3. Switch Firmware");
        LinearLayout.LayoutParams p3 = new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1.0f);
        p3.leftMargin = dp(6);
        fwCard.setLayoutParams(p3);

        mFirmwareStatusText = new TextView(this);
        mFirmwareStatusText.setTextSize(TypedValue.COMPLEX_UNIT_SP, 11);
        mFirmwareStatusText.setPadding(0, dp(2), 0, dp(8));
        fwCard.addView(mFirmwareStatusText);

        LinearLayout fwRow = new LinearLayout(this);
        fwRow.setOrientation(LinearLayout.HORIZONTAL);

        Button btnFw = createStyledButton("Install (.zip)", "#8957E5");
        LinearLayout.LayoutParams f1 = new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1.0f);
        f1.rightMargin = dp(4);
        btnFw.setLayoutParams(f1);
        btnFw.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                playSound("button_click.wav");
                pickDocument(REQ_FIRMWARE, "*/*");
            }
        });
        fwRow.addView(btnFw);

        Button btnFwHelp = createStyledButton("Info", "#21262D");
        LinearLayout.LayoutParams f2 = new LinearLayout.LayoutParams(dp(54), ViewGroup.LayoutParams.WRAP_CONTENT);
        btnFwHelp.setLayoutParams(f2);
        btnFwHelp.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                showInfoDialog(
                    "Nintendo Switch Firmware",
                    "Firmware provides system shared fonts (NintendoStandard.ttf), Mii rendering databases, and timezone archives.\n\n"
                    + "• Status: The app already bundles built-in Nintendo-compatible fallback fonts and Mii data!\n"
                    + "• Optional: If you have your console's Firmware 18.x.zip, tap 'Install (.zip)' to extract full official system archives."
                );
            }
        });
        fwRow.addView(btnFwHelp);
        fwCard.addView(fwRow);
        panelsRow.addView(fwCard);

        root.addView(panelsRow);

        // 4. PROGRESS BAR & STATUS LABEL
        mProgressLabel = new TextView(this);
        mProgressLabel.setTextColor(Color.parseColor("#58A6FF"));
        mProgressLabel.setTextSize(TypedValue.COMPLEX_UNIT_SP, 12);
        mProgressLabel.setVisibility(View.GONE);
        mProgressLabel.setPadding(0, dp(2), 0, dp(2));
        root.addView(mProgressLabel);

        mProgressBar = new ProgressBar(this, null, android.R.attr.progressBarStyleHorizontal);
        mProgressBar.setIndeterminate(false);
        mProgressBar.setVisibility(View.GONE);
        root.addView(mProgressBar);

        // 5. ACTION BUTTONS ROW
        LinearLayout actionRow = new LinearLayout(this);
        actionRow.setOrientation(LinearLayout.HORIZONTAL);
        actionRow.setPadding(0, dp(12), 0, 0);

        mLaunchButton = createStyledButton("▶ PLAY TOMODACHI LIFE", "#238636");
        LinearLayout.LayoutParams launchP = new LinearLayout.LayoutParams(0, dp(50), 1.6f);
        launchP.rightMargin = dp(8);
        mLaunchButton.setLayoutParams(launchP);
        mLaunchButton.setTextSize(TypedValue.COMPLEX_UNIT_SP, 15);
        mLaunchButton.setTypeface(Typeface.create("sans-serif-black", Typeface.BOLD));
        mLaunchButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                if (checkKeysExist(MainActivity.this) && checkGameExists(MainActivity.this)) {
                    playSound("game_start.wav");
                    launchGameEngine();
                } else {
                    showMissingAssetsAlert();
                }
            }
        });
        actionRow.addView(mLaunchButton);

        mTestModeButton = createStyledButton("🎮 Viewport & Controller Test", "#8957E5");
        LinearLayout.LayoutParams testP = new LinearLayout.LayoutParams(0, dp(50), 1.0f);
        testP.rightMargin = dp(8);
        mTestModeButton.setLayoutParams(testP);
        mTestModeButton.setTextSize(TypedValue.COMPLEX_UNIT_SP, 12);
        mTestModeButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                playSound("switch_snap.wav");
                Toast.makeText(MainActivity.this, "Testing 60 FPS Viewport & Virtual Joy-Cons...", Toast.LENGTH_SHORT).show();
                launchGameEngine();
            }
        });
        actionRow.addView(mTestModeButton);

        Button btnRefresh = createStyledButton("🔄 Refresh", "#21262D");
        LinearLayout.LayoutParams refP = new LinearLayout.LayoutParams(dp(90), dp(50));
        btnRefresh.setLayoutParams(refP);
        btnRefresh.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                playSound("button_click.wav");
                refreshAllStatus();
                Toast.makeText(MainActivity.this, "Storage status re-scanned.", Toast.LENGTH_SHORT).show();
            }
        });
        actionRow.addView(btnRefresh);

        root.addView(actionRow);

        scrollView.addView(root);
        setContentView(scrollView);
    }

    private LinearLayout createGameCard() {
        LinearLayout card = new LinearLayout(this);
        card.setOrientation(LinearLayout.HORIZONTAL);
        card.setPadding(dp(14), dp(10), dp(14), dp(10));
        card.setBackground(createRoundedBackground("#161B22", "#30363D", 10));
        card.setGravity(Gravity.CENTER_VERTICAL);
        LinearLayout.LayoutParams cardParams = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        cardParams.topMargin = dp(8);
        cardParams.bottomMargin = dp(4);
        card.setLayoutParams(cardParams);

        mGameIconView = new ImageView(this);
        mGameIconView.setImageResource(android.R.drawable.sym_def_app_icon);
        LinearLayout.LayoutParams iconP = new LinearLayout.LayoutParams(dp(54), dp(54));
        iconP.rightMargin = dp(14);
        mGameIconView.setLayoutParams(iconP);
        card.addView(mGameIconView);

        LinearLayout textCol = new LinearLayout(this);
        textCol.setOrientation(LinearLayout.VERTICAL);
        LinearLayout.LayoutParams textColP = new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1.0f);
        textCol.setLayoutParams(textColP);

        mGameTitleText = new TextView(this);
        mGameTitleText.setText("Tomodachi Life: Living the Dream");
        mGameTitleText.setTextColor(Color.parseColor("#F0F6FC"));
        mGameTitleText.setTextSize(TypedValue.COMPLEX_UNIT_SP, 15);
        mGameTitleText.setTypeface(Typeface.create("sans-serif-medium", Typeface.BOLD));
        textCol.addView(mGameTitleText);

        mGameMetaText = new TextView(this);
        mGameMetaText.setText("Title ID: 0100D77011B8E000 • Version 1.0.0");
        mGameMetaText.setTextColor(Color.parseColor("#8B949E"));
        mGameMetaText.setTextSize(TypedValue.COMPLEX_UNIT_SP, 11);
        mGameMetaText.setPadding(0, dp(2), 0, dp(2));
        textCol.addView(mGameMetaText);

        card.addView(textCol);

        mGameBadgeText = new TextView(this);
        mGameBadgeText.setText("Awaiting Game");
        mGameBadgeText.setTextSize(TypedValue.COMPLEX_UNIT_SP, 11);
        mGameBadgeText.setTextColor(Color.parseColor("#E3B341"));
        mGameBadgeText.setPadding(dp(10), dp(4), dp(10), dp(4));
        mGameBadgeText.setBackground(createRoundedBackground("#21262D", "#30363D", 6));
        card.addView(mGameBadgeText);

        return card;
    }

    private void refreshAllStatus() {
        boolean hasKeys = checkKeysExist(this);
        boolean hasFw = checkFirmwareExist(this);
        boolean hasGame = checkGameExists(this);

        if (mKeysStatusText != null) {
            if (hasKeys) {
                File k = new File(getKeysDir(this), "prod.keys");
                mKeysStatusText.setText("✅ prod.keys (" + (k.length() / 1024) + " KB ready)");
                mKeysStatusText.setTextColor(Color.parseColor("#39D353"));
            } else {
                mKeysStatusText.setText("❌ Missing prod.keys");
                mKeysStatusText.setTextColor(Color.parseColor("#F85149"));
            }
        }

        if (mFirmwareStatusText != null) {
            if (hasFw) {
                mFirmwareStatusText.setText("✅ Firmware Ready (Installed)");
                mFirmwareStatusText.setTextColor(Color.parseColor("#39D353"));
            } else {
                mFirmwareStatusText.setText("ℹ️ Built-in Fonts & Assets");
                mFirmwareStatusText.setTextColor(Color.parseColor("#58A6FF"));
            }
        }

        if (mGameStatusText != null) {
            File gamesDir = getGamesDir(this);
            File xci = new File(gamesDir, "tomodachi_game.xci");
            File nsp = new File(gamesDir, "tomodachi_game.nsp");
            File oldNsp = new File(gamesDir, "tomodachi.nsp");

            if (xci.exists() && xci.length() > 0) {
                long mb = xci.length() / (1024 * 1024);
                mGameStatusText.setText("✅ XCI GameCard (" + mb + " MB mounted)");
                mGameStatusText.setTextColor(Color.parseColor("#39D353"));
                mGameBadgeText.setText("XCI MOUNTED");
                mGameBadgeText.setTextColor(Color.parseColor("#39D353"));
            } else if ((nsp.exists() && nsp.length() > 0) || (oldNsp.exists() && oldNsp.length() > 0)) {
                long mb = (nsp.exists() ? nsp.length() : oldNsp.length()) / (1024 * 1024);
                mGameStatusText.setText("✅ NSP Package (" + mb + " MB mounted)");
                mGameStatusText.setTextColor(Color.parseColor("#39D353"));
                mGameBadgeText.setText("NSP MOUNTED");
                mGameBadgeText.setTextColor(Color.parseColor("#39D353"));
            } else if (hasGame) {
                mGameStatusText.setText("✅ RomFS Mounted");
                mGameStatusText.setTextColor(Color.parseColor("#39D353"));
                mGameBadgeText.setText("RomFS READY");
                mGameBadgeText.setTextColor(Color.parseColor("#39D353"));
            } else {
                mGameStatusText.setText("❌ Missing .XCI / .NSP");
                mGameStatusText.setTextColor(Color.parseColor("#F85149"));
                mGameBadgeText.setText("NOT MOUNTED");
                mGameBadgeText.setTextColor(Color.parseColor("#F85149"));
            }
        }

        if (mLaunchButton != null) {
            boolean ready = hasKeys && hasGame;
            if (ready) {
                mLaunchButton.setText("▶ PLAY TOMODACHI LIFE");
                mLaunchButton.setBackground(createRoundedBackground("#238636", "#2EA043", 8));
            } else {
                mLaunchButton.setText("⚠️ Complete Setup to Play");
                mLaunchButton.setBackground(createRoundedBackground("#21262D", "#30363D", 8));
            }
        }
    }

    private void launchGameEngine() {
        Intent intent = new Intent(this, GameActivity.class);
        startActivity(intent);
    }

    private void showMissingAssetsAlert() {
        new AlertDialog.Builder(this)
            .setTitle("Asset Setup Required")
            .setMessage(
                "To run Tomodachi Life Recomp, the engine requires:\n\n"
                + (!checkKeysExist(this) ? "• prod.keys (Missing) - Tap 'prod.keys' button to import.\n" : "• prod.keys (Ready)\n")
                + (!checkGameExists(this) ? "• Game Dump (Missing) - Tap 'Install .XCI / .NSP' to import your game card or digital dump.\n\n" : "• Game Dump (Ready)\n\n")
                + "Would you like to enter Viewport Test Mode to test the graphics pipeline and virtual gamepad?"
            )
            .setPositiveButton("Enter Test Mode", new DialogInterface.OnClickListener() {
                @Override
                public void onClick(DialogInterface dialog, int which) {
                    launchGameEngine();
                }
            })
            .setNegativeButton("Back", null)
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

        final Uri uri = data.getData();
        if (uri == null) return;

        if (requestCode == REQ_KEYS) {
            importSimpleFileAsync(uri, new File(getKeysDir(this), "prod.keys"), "prod.keys");
        } else if (requestCode == REQ_TITLE_KEYS) {
            importSimpleFileAsync(uri, new File(getKeysDir(this), "title.keys"), "title.keys");
        } else if (requestCode == REQ_GAME_PKG) {
            processGamePackage(uri);
        } else if (requestCode == REQ_FIRMWARE) {
            processFirmwarePackage(uri);
        } else if (requestCode == REQ_ROMFS_DIR) {
            importRomfsFolderAsync(uri);
        }
    }

    private void processGamePackage(Uri uri) {
        showProgress("Analyzing Switch container (XCI / NSP)...", 0);
        SwitchPackageExtractor.processPackageAsync(this, uri, new SwitchPackageExtractor.ProgressCallback() {
            @Override
            public void onProgress(final String status, final int percent) {
                mHandler.post(new Runnable() {
                    @Override
                    public void run() {
                        if (mProgressLabel != null) {
                            mProgressLabel.setText(status + " (" + percent + "%)");
                            mProgressLabel.setVisibility(View.VISIBLE);
                        }
                        if (mProgressBar != null) {
                            mProgressBar.setIndeterminate(false);
                            mProgressBar.setProgress(percent);
                            mProgressBar.setVisibility(View.VISIBLE);
                        }
                    }
                });
            }

            @Override
            public void onSuccess(final SwitchPackageExtractor.GameInfo info) {
                mHandler.post(new Runnable() {
                    @Override
                    public void run() {
                        playSound("switch_snap.wav");
                        hideProgress();
                        if (info.icon != null && mGameIconView != null) {
                            mGameIconView.setImageBitmap(info.icon);
                        }
                        Toast.makeText(MainActivity.this, info.format + " extracted and mounted successfully!", Toast.LENGTH_LONG).show();
                        refreshAllStatus();
                    }
                });
            }

            @Override
            public void onError(final String error) {
                mHandler.post(new Runnable() {
                    @Override
                    public void run() {
                        hideProgress();
                        Toast.makeText(MainActivity.this, error, Toast.LENGTH_LONG).show();
                        refreshAllStatus();
                    }
                });
            }
        });
    }

    private void processFirmwarePackage(Uri uri) {
        showProgress("Installing Nintendo Switch Firmware...", 0);
        SwitchPackageExtractor.installFirmwareAsync(this, uri, new SwitchPackageExtractor.ProgressCallback() {
            @Override
            public void onProgress(final String status, final int percent) {
                mHandler.post(new Runnable() {
                    @Override
                    public void run() {
                        if (mProgressLabel != null) {
                            mProgressLabel.setText(status);
                            mProgressLabel.setVisibility(View.VISIBLE);
                        }
                        if (mProgressBar != null) {
                            mProgressBar.setIndeterminate(true);
                            mProgressBar.setVisibility(View.VISIBLE);
                        }
                    }
                });
            }

            @Override
            public void onSuccess(final SwitchPackageExtractor.GameInfo info) {
                mHandler.post(new Runnable() {
                    @Override
                    public void run() {
                        playSound("switch_snap.wav");
                        hideProgress();
                        Toast.makeText(MainActivity.this, "Firmware installed (" + info.fileCount + " system archives)!", Toast.LENGTH_SHORT).show();
                        refreshAllStatus();
                    }
                });
            }

            @Override
            public void onError(final String error) {
                mHandler.post(new Runnable() {
                    @Override
                    public void run() {
                        hideProgress();
                        Toast.makeText(MainActivity.this, error, Toast.LENGTH_LONG).show();
                        refreshAllStatus();
                    }
                });
            }
        });
    }

    private void importSimpleFileAsync(final Uri uri, final File destFile, final String label) {
        showProgress("Importing " + label + "...", 0);
        new Thread(new Runnable() {
            @Override
            public void run() {
                try {
                    InputStream in = getContentResolver().openInputStream(uri);
                    FileOutputStream out = new FileOutputStream(destFile);
                    byte[] buffer = new byte[65536];
                    int read;
                    while ((read = in.read(buffer)) != -1) {
                        out.write(buffer, 0, read);
                    }
                    in.close();
                    out.flush();
                    out.close();

                    mHandler.post(new Runnable() {
                        @Override
                        public void run() {
                            playSound("switch_snap.wav");
                            hideProgress();
                            Toast.makeText(MainActivity.this, label + " imported (" + (destFile.length() / 1024) + " KB)!", Toast.LENGTH_SHORT).show();
                            refreshAllStatus();
                        }
                    });
                } catch (final Exception e) {
                    mHandler.post(new Runnable() {
                        @Override
                        public void run() {
                            hideProgress();
                            Toast.makeText(MainActivity.this, "Error importing " + label + ": " + e.getMessage(), Toast.LENGTH_LONG).show();
                        }
                    });
                }
            }
        }).start();
    }

    private void importRomfsFolderAsync(final Uri treeUri) {
        showProgress("Copying RomFS folder structure...", 0);
        new Thread(new Runnable() {
            @Override
            public void run() {
                try {
                    DocumentFile rootDir = DocumentFile.fromTreeUri(MainActivity.this, treeUri);
                    File targetRomfs = getRomFsDir(MainActivity.this);
                    if (rootDir != null) {
                        copyDocTree(rootDir, targetRomfs);
                    }
                    mHandler.post(new Runnable() {
                        @Override
                        public void run() {
                            playSound("switch_snap.wav");
                            hideProgress();
                            Toast.makeText(MainActivity.this, "RomFS directory tree imported successfully!", Toast.LENGTH_SHORT).show();
                            refreshAllStatus();
                        }
                    });
                } catch (final Exception e) {
                    mHandler.post(new Runnable() {
                        @Override
                        public void run() {
                            hideProgress();
                            Toast.makeText(MainActivity.this, "RomFS copy error: " + e.getMessage(), Toast.LENGTH_LONG).show();
                        }
                    });
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
                byte[] buf = new byte[65536];
                int read;
                while ((read = in.read(buf)) != -1) {
                    out.write(buf, 0, read);
                }
                in.close();
                out.flush();
                out.close();
            }
        }
    }

    private void showProgress(final String label, final int percent) {
        mHandler.post(new Runnable() {
            @Override
            public void run() {
                if (mProgressLabel != null) {
                    mProgressLabel.setText(label);
                    mProgressLabel.setVisibility(View.VISIBLE);
                }
                if (mProgressBar != null) {
                    mProgressBar.setIndeterminate(percent <= 0);
                    if (percent > 0) mProgressBar.setProgress(percent);
                    mProgressBar.setVisibility(View.VISIBLE);
                }
            }
        });
    }

    private void hideProgress() {
        if (mProgressBar != null) mProgressBar.setVisibility(View.GONE);
        if (mProgressLabel != null) mProgressLabel.setVisibility(View.GONE);
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

        String vkString = hasVulkan ? ("Vulkan 1." + ((vkVer >> 12) & 0x3ff) + " Active") : "OpenGL ES 3.0 Fallback";
        return "ABI: " + abi + "  •  " + vkString + "  •  Android " + Build.VERSION.RELEASE;
    }

    private LinearLayout createPanelCard(String headerTitle) {
        LinearLayout card = new LinearLayout(this);
        card.setOrientation(LinearLayout.VERTICAL);
        card.setPadding(dp(12), dp(10), dp(12), dp(10));
        card.setBackground(createRoundedBackground("#161B22", "#30363D", 8));

        TextView label = new TextView(this);
        label.setText(headerTitle);
        label.setTextColor(Color.parseColor("#F0F6FC"));
        label.setTextSize(TypedValue.COMPLEX_UNIT_SP, 12);
        label.setTypeface(Typeface.create("sans-serif-medium", Typeface.BOLD));
        label.setPadding(0, 0, 0, dp(4));
        card.addView(label);

        return card;
    }

    private Button createStyledButton(String text, String colorHex) {
        Button btn = new Button(this);
        btn.setText(text);
        btn.setTextColor(Color.WHITE);
        btn.setTextSize(TypedValue.COMPLEX_UNIT_SP, 11);
        btn.setBackground(createRoundedBackground(colorHex, colorHex, 6));
        return btn;
    }

    private GradientDrawable createRoundedBackground(String fillColor, String strokeColor, int radiusDp) {
        GradientDrawable gd = new GradientDrawable();
        gd.setColor(Color.parseColor(fillColor));
        gd.setStroke(dp(1), Color.parseColor(strokeColor));
        gd.setCornerRadius(dp(radiusDp));
        return gd;
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

    @Override
    protected void onDestroy() {
        if (mMediaPlayer != null) {
            mMediaPlayer.release();
            mMediaPlayer = null;
        }
        super.onDestroy();
    }
}
