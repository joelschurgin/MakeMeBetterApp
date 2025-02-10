/*
 * Copyright 2015 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.ece420.makeMeBetter;

import static java.lang.Math.random;
import static java.lang.Math.round;

import android.app.Activity;
import android.content.Context;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Rect;
import android.Manifest;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioRecord;
import android.os.AsyncTask;
import android.os.Bundle;
import android.os.Handler;
import android.support.annotation.NonNull;
import android.support.v4.app.ActivityCompat;
import android.view.KeyEvent;
import android.view.Menu;
import android.view.MenuItem;
import android.view.MotionEvent;
import android.view.View;
import android.view.WindowManager;
import android.view.inputmethod.EditorInfo;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.CompoundButton;
import android.widget.EditText;
import android.widget.Spinner;
import android.widget.Switch;
import android.widget.ImageView;
import android.widget.SeekBar;
import android.widget.TextView;
import android.widget.Toast;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.FloatBuffer;
import java.util.ArrayList;
import java.util.Random;
import java.util.Timer;
import java.util.TimerTask;
import java.util.stream.IntStream;


public class MainActivity extends Activity
        implements ActivityCompat.OnRequestPermissionsResultCallback {

    // UI Variables
    Button controlButton;
    TextView notesInputRepeated;
    EditText notesInput;
    Button submitButton;
    SeekBar tempoSeekBar;
    TextView tempo_TextView;
    TextView envelope_TextView;
    SeekBar envelopeSeekBar;
    TextView cutoff_TextView;
    SeekBar cutoffSeekBar;
    TextView Q_TextView;
    SeekBar QSeekBar;
    Spinner randomLength;
    Button randomButton;


    int randomMelodyNumNotes = 8;
    Button recordButton;
    Button recordPlaybackButton;
    Switch recordModeSwitch;

    final int NumMelodies = 3;
    ArrayList<String> melodyStrings = new ArrayList<String>(NumMelodies);
    int melodyIdx = 0;

    String  nativeSampleRate;
    String  nativeSampleBufSize;
    boolean supportRecording;
    Boolean isPlaying = false;
    ImageView stftView;
    Bitmap bitmap;
    Canvas canvas;
    Paint paint;

    Boolean switchState;

    // Static Values
    private static final int AUDIO_ECHO_REQUEST = 0;
    private static final int FRAME_SIZE = 1024;
    private static final int BITMAP_HEIGHT = 500;
    private static final int MIN_FREQ = 50;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        setContentView(R.layout.activity_main);
        super.setRequestedOrientation (ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);

        // Google NDK Stuff
        controlButton = (Button)findViewById((R.id.capture_control_button));

        queryNativeAudioParameters();
        // initialize native audio system
        updateNativeAudioUI();
        if (supportRecording) {
            // Change audio sampling rate and frame size
            createSLEngine(Integer.parseInt(nativeSampleRate), FRAME_SIZE);
        }

        // set up notesInputRepeated
        notesInputRepeated = (TextView) findViewById(R.id.notesInputRepeated);
        notesInputRepeated.setText("...waiting");

        // set up notesInput
        notesInput = (EditText) findViewById(R.id.notesInput);
        notesInput.setOnEditorActionListener(new TextView.OnEditorActionListener() {
            @Override
            public boolean onEditorAction(TextView v, int actionId, KeyEvent keyEvent) {
                boolean handled = false;
                if (actionId == EditorInfo.IME_ACTION_DONE || actionId == EditorInfo.IME_ACTION_GO) {
                    handleTextInput();
                    handled = true;
                }
                return handled;
            }
        });

        // set up submitButton
        submitButton = (Button) findViewById(R.id.submitButton);
        submitButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                handleTextInput();
            }
        });

        //set up tempo_TextView
        tempo_TextView = (TextView) findViewById(R.id.tempo_TextView);
        tempo_TextView.setText("Tempo: ");

        // set up tempoSeekBar
        tempoSeekBar = (SeekBar) findViewById(R.id.tempoSeekBar);
        tempoSeekBar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int i, boolean b) {
                int newTempo = (int)(40.0f + 2.405f * (float)i);        // BPM From 40 to 280
                tempo_TextView.setText("Tempo: " + Integer.toString(newTempo) + " BPM");
                writeNewTempo(newTempo);
            }

            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {}

            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {}
        });
        tempoSeekBar.setProgress(10);

        //set up envelope_TextView
        envelope_TextView = (TextView) findViewById(R.id.envelope_TextView);

        //set upt envelopeSeekBar
        envelopeSeekBar = (SeekBar) findViewById(R.id.envelopeSeekBar);
        envelopeSeekBar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int i, boolean b) {
                writeNewEnvelopePeakPosition((float)i / 100.0f);
            }

            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {}

            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {}
        });
        envelopeSeekBar.setProgress(10);

        cutoff_TextView = (TextView) findViewById(R.id.cutoff_TextView);
        cutoffSeekBar = (SeekBar) findViewById(R.id.cutoffSeekbar);
        cutoffSeekBar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int i, boolean b) {
                final float MaxOctaves = 8.0f;
                final float Octave = ((float)i / 100.0f) * MaxOctaves;
                cutoff_TextView.setText("Filter Cutoff (Octaves): " + Float.toString(Octave));
                setFilterCutoff(Octave);
            }

            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {}

            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {}
        });

        cutoffSeekBar.setProgress(25);

        Q_TextView = (TextView) findViewById(R.id.Q_TextView);
        QSeekBar = (SeekBar) findViewById(R.id.QSeekBar);
        QSeekBar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int i, boolean b) {
                final float MaxQ = 8.0f;
                final float CenterQ = 1.0f; // center of seek bar should be this value
                final float MinQ = 0.1f;

                final float Q = (i >= 50) ? ((float)(i - 50) / 50.0f) * (MaxQ - CenterQ) + CenterQ : ((float)i / 50.0f) * (CenterQ - MinQ) + MinQ;
                Q_TextView.setText("Filter Resonance: " + Float.toString(round(Q * 100.0f) / 100.0f));
                setFilterQ(Q);
            }

            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {}

            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {}
        });

        QSeekBar.setProgress(50);

        randomLength = (Spinner) findViewById(R.id.randomLength);
        ArrayList<String> randomLengthItems = new ArrayList<String>();
        randomLengthItems.add("1 Bar");
        randomLengthItems.add("2 Bars");
        randomLengthItems.add("4 Bars");

        ArrayAdapter<String> randomLengthAdapter = new ArrayAdapter<>(this, android.R.layout.simple_spinner_dropdown_item, randomLengthItems);
        randomLength.setAdapter(randomLengthAdapter);
        randomLength.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
           @Override
           public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
               final int NumBars = 1 << position; // 2 ^ position
               randomMelodyNumNotes = NumBars * 8;
           }

            @Override
            public void onNothingSelected(AdapterView<?> adapterView) {

            }
        });

        randomButton = (Button) findViewById(R.id.randomButton);
        randomButton.setOnClickListener(new View.OnClickListener() {
            Random random = new Random();
            @Override
            public void onClick(View view) {
                String possibleChars = "12345678xx";
                String randomMelody = "";
                for (int i = 0; i < randomMelodyNumNotes; ++i) {
                    final int idx = random.nextInt(possibleChars.length());
                    randomMelody += possibleChars.charAt(idx);
                }
                notesInput.setText(randomMelody);
            }
        });

        recordButton = (Button) findViewById(R.id.recordButton);
        recordButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                if (recordButton.getText() == "Record") {
                    recordButton.setText("Stop");
                    recordButton.setBackgroundColor(Color.RED);
                    recordButton.setTextColor(Color.WHITE);
                    if (!isPlaying)
                        startEcho();
                    startRecord();
                } else {
                    recordButton.setText("Record");
                    recordButton.setBackgroundColor(Color.GREEN);
                    recordButton.setTextColor(Color.BLACK);
                    stopRecord();
                }
            }
        });
        recordButton.callOnClick();

        recordPlaybackButton = (Button) findViewById(R.id.recordPlaybackButton);
        recordPlaybackButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                if (recordPlaybackButton.getText() != "Play") {
                    recordPlaybackButton.setText("Play");
                    stopRecord();
                } else {
                    recordPlaybackButton.setText("Stop");
                    playRecording();
                }
            }
        });
        recordPlaybackButton.callOnClick();

        recordModeSwitch = (Switch) findViewById(R.id.recordModeSwitch);
        recordModeSwitch.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
            @Override
            public void onCheckedChanged(CompoundButton compoundButton, boolean isChecked) {
                setRecordMode(isChecked);
                if (isChecked) {
                    recordButton.setVisibility(View.VISIBLE);
                    recordPlaybackButton.setVisibility(View.VISIBLE);

                    recordButton.setText("");
                    recordButton.callOnClick();

                    recordPlaybackButton.setText("");
                    recordPlaybackButton.callOnClick();

                    if (!isPlaying)
                        startEcho();
                } else {
                    recordButton.setVisibility(View.INVISIBLE);
                    recordPlaybackButton.setVisibility(View.INVISIBLE);
                    stopRecord();
                    if (isPlaying)
                        startEcho();
                }
            }
        });
        recordModeSwitch.setChecked(true);

        // Copied from OnClick handler
        if (ActivityCompat.checkSelfPermission(this, Manifest.permission.RECORD_AUDIO) !=
                PackageManager.PERMISSION_GRANTED) {
            ActivityCompat.requestPermissions(
                    this,
                    new String[] { Manifest.permission.RECORD_AUDIO },
                    AUDIO_ECHO_REQUEST);
            return;
        }
//        startEcho();
    }
    @Override
    protected void onDestroy() {
        if (supportRecording) {
            if (isPlaying) {
                stopPlay();
            }
            deleteSLEngine();
            isPlaying = false;
        }
        super.onDestroy();
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        // Inflate the menu; this adds items to the action bar if it is present.
        getMenuInflater().inflate(R.menu.menu_main, menu);
        return true;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        // Handle action bar item clicks here. The action bar will
        // automatically handle clicks on the Home/Up button, so long
        // as you specify a parent activity in AndroidManifest.xml.
        int id = item.getItemId();

        //noinspection SimplifiableIfStatement
        if (id == R.id.action_settings) {
            return true;
        }

        return super.onOptionsItemSelected(item);
    }

    private void handleTextInput() {
        String sequence = notesInput.getText().toString();

        // check for valid input
        String validChars = "12345678xX";
        for (char c : sequence.toCharArray()) {
            if (validChars.indexOf(c) < 0) {
                notesInputRepeated.setText("ERROR: Input Must Only Contain 1-8 or x");
                return;
            }
        }
        notesInputRepeated.setText("Submitted!");
        getNotesInput(sequence);
    }

    private void startEcho() {
        if(!supportRecording){
            return;
        }
        if (!isPlaying) {
            if(!createSLBufferQueueAudioPlayer()) {
                return;
            }
            if(!createAudioRecorder()) {
                deleteSLBufferQueueAudioPlayer();
                return;
            }
            startPlay();   // this must include startRecording()
        } else {
            stopPlay();  //this must include stopRecording()
            updateNativeAudioUI();
            deleteAudioRecorder();
            deleteSLBufferQueueAudioPlayer();
        }
        isPlaying = !isPlaying;
        controlButton.setText(getString((isPlaying == true) ?
                R.string.StopEcho: R.string.StartEcho));
    }

    public void onEchoClick(View view) {
        if (ActivityCompat.checkSelfPermission(this, Manifest.permission.RECORD_AUDIO) !=
                PackageManager.PERMISSION_GRANTED) {
            ActivityCompat.requestPermissions(
                    this,
                    new String[] { Manifest.permission.RECORD_AUDIO },
                    AUDIO_ECHO_REQUEST);
            return;
        }
        startEcho();
    }

    public void getLowLatencyParameters(View view) {
        updateNativeAudioUI();
        return;
    }

    private void queryNativeAudioParameters() {
        AudioManager myAudioMgr = (AudioManager) getSystemService(Context.AUDIO_SERVICE);
        nativeSampleRate  =  myAudioMgr.getProperty(AudioManager.PROPERTY_OUTPUT_SAMPLE_RATE);
        nativeSampleBufSize =myAudioMgr.getProperty(AudioManager.PROPERTY_OUTPUT_FRAMES_PER_BUFFER);
        int recBufSize = AudioRecord.getMinBufferSize(
                Integer.parseInt(nativeSampleRate),
                AudioFormat.CHANNEL_IN_MONO,
                AudioFormat.ENCODING_PCM_16BIT);
        supportRecording = true;
        if (recBufSize == AudioRecord.ERROR ||
                recBufSize == AudioRecord.ERROR_BAD_VALUE) {
            supportRecording = false;
        }
    }
    private void updateNativeAudioUI() {
        if (!supportRecording) {
            controlButton.setEnabled(false);

            return;
        }


    }
    @Override
    public void onRequestPermissionsResult(int requestCode, @NonNull String[] permissions,
                                           @NonNull int[] grantResults) {
        /*
         * if any permission failed, the sample could not play
         */
        if (AUDIO_ECHO_REQUEST != requestCode) {
            super.onRequestPermissionsResult(requestCode, permissions, grantResults);
            return;
        }

        if (grantResults.length != 1  ||
                grantResults[0] != PackageManager.PERMISSION_GRANTED) {
            /*
             * When user denied permission, throw a Toast to prompt that RECORD_AUDIO
             * is necessary; also display the status on UI
             * Then application goes back to the original state: it behaves as if the button
             * was not clicked. The assumption is that user will re-click the "start" button
             * (to retry), or shutdown the app in normal way.
             */
            Toast.makeText(getApplicationContext(),
                    getString(R.string.prompt_permission),
                    Toast.LENGTH_SHORT).show();
            return;
        }

        /*
         * When permissions are granted, we prompt the user the status. User would
         * re-try the "start" button to perform the normal operation. This saves us the extra
         * logic in code for async processing of the button listener.
         */

        // The callback runs on app's thread, so we are safe to resume the action
        startEcho();
    }

    /*
     * Loading our Libs
     */
    static {
        System.loadLibrary("echo");
    }

    /*
     * jni function implementations...
     */
    public static native void createSLEngine(int rate, int framesPerBuf);
    public static native void deleteSLEngine();

    public static native boolean createSLBufferQueueAudioPlayer();
    public static native void deleteSLBufferQueueAudioPlayer();

    public static native boolean createAudioRecorder();
    public static native void deleteAudioRecorder();
    public static native void startPlay();
    public static native void stopPlay();
    public static native void getNotesInput(String input);
    public static native void writeNewTempo(int tempo);
    public static native void writeNewEnvelopePeakPosition(float position);
    public static native void setFilterCutoff(float cutoff);
    public static native void setFilterQ(float Q);

    public static native void setRecordMode(boolean recordModeOn);
    public static native void startRecord();
    public static native void stopRecord();

    public static native void playRecording();
}
