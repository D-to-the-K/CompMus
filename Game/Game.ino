#include "LedControl.h"

class Time {
public:
  unsigned int minutes;
  unsigned int seconds;
  unsigned int ms;

  Time(unsigned int minutes, unsigned int seconds, unsigned int ms) {
    this->minutes = minutes;
    this->seconds = seconds;
    this->ms = ms;
  }

  unsigned int toMs() {
    return minutes * 60 * 1000 + seconds * 1000 + ms;
  }
};

class Note {
public:
  byte track;
  Time startTime = Time(0, 0, 0);
  byte channel;
  unsigned int frequency;
  unsigned long durationMs;

  Note(byte track, Time startTime, byte channel, unsigned int frequency, unsigned long durationMs) {
    this->track = track;
    this->startTime = startTime;
    this->channel = channel;
    this->frequency = frequency;
    this->durationMs = durationMs;
  }
};

Note songNotes[] = {
  Note(0, Time(0, 0, 0), 0, 1500, 500),
  Note(0, Time(0, 1, 0), 0, 500, 250),
  Note(0, Time(0, 2, 0), 0, 1500, 500),
  Note(0, Time(0, 3, 0), 0, 500, 250),

  Note(1, Time(0, 4, 0), 0, 1500, 500),
  Note(1, Time(0, 5, 0), 0, 500, 250),
  Note(1, Time(0, 6, 0), 0, 1500, 500),
  Note(1, Time(0, 7, 0), 0, 500, 250)
};

// game/song params
const int GLOBAL_DELAY_MS = 3000; // used to adjust both song and notes together
const int TIMING_WINDOW_MS = 100; // how many ms can the player hit the note before or after the note's time
const int BEATS_PER_MINUTE = 120; // song's BPM
const int NUM_KEYS = 2; // number of player input keys
const int NUM_ROWS = 3; // number of rows in the playfield
const float NOTE_SPEED = 2; // number of rows of separation between consecutive notes
const int BEAT_DURATION_MS = 60.0 / BEATS_PER_MINUTE * 1000;
const int PLAYFIELD_DRAW_DISTANCE_MS = NUM_ROWS / NOTE_SPEED * BEAT_DURATION_MS;
const unsigned int LAST_NOTE = sizeof(songNotes) / sizeof(songNotes[0]);
const int POINTS_ON_HIT = 15;

// pins
const int BUZZER_PIN = 3; // PWM pin for the buzzer
const int JUDGE_RGB_LED_R_PIN = 11; // red pin for an RGB LED (used to tell the player if he hit or missed the note)
const int JUDGE_RGB_LED_G_PIN = 10; // green pin for an RGB LED
const int JUDGE_RGB_LED_B_PIN = 9; // blue pin for an RGB LED
const int JUDGE_LED_RED = 6;
const int JUDGE_LED_GREEN = 5;
const int KEY_0_PIN = 4; // pin for the first game button (leftmost)
const int KEY_1_PIN = 2; // pin for the second game button

// colors
const byte RGB_BLACK[] = {0, 0, 0};
const byte RGB_WHITE[] = {255, 255, 255};
const byte RGB_RED[] = {255, 0, 0};
const byte RGB_GREEN[] = {0, 255, 0};
const byte RGB_BLUE[] = {0, 0, 255};



// game state
unsigned int nextNoteToJudge = 0;
unsigned int gameStartMillis = 0;
bool lastSeenKeys[NUM_KEYS] = {0};
bool playField[NUM_ROWS][NUM_KEYS] = {0};
int playFieldPins [NUM_ROWS][NUM_KEYS] = {{A0, A3},{A1,A4},{A2,A5}};
int points = 0;
LedControl lc=LedControl(12,11,10,1);

bool isKeyPressed(int key) {
  switch (key) {
    case 0:
      return digitalRead(KEY_0_PIN) == LOW;
    case 1:
      return digitalRead(KEY_1_PIN) == LOW;
    // TODO: add more
    default:
      Serial.print("ERROR: unknown key in function isKeyPressed():");
      Serial.println(key);
      return false;
  }
}

void setJudgeColor(const byte color[]) {
  analogWrite(JUDGE_RGB_LED_R_PIN, 255 - color[0]);
  analogWrite(JUDGE_RGB_LED_G_PIN, 255 - color[1]);
  analogWrite(JUDGE_RGB_LED_B_PIN, 255 - color[2]);
}

void judgeMiss() {
  // TODO: blink and then reset?
  Serial.print("MISS\n");
  setJudgeColor(RGB_RED);
  noTone(BUZZER_PIN);
}

void judgeHit(const Note& note) {
  // TODO: blink and then reset?
  setJudgeColor(RGB_GREEN);
  tone(BUZZER_PIN, note.frequency, note.durationMs);
  points += POINTS_ON_HIT;
}

void setup() {

  
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(JUDGE_RGB_LED_R_PIN, OUTPUT);
  pinMode(JUDGE_RGB_LED_G_PIN, OUTPUT);
  pinMode(JUDGE_RGB_LED_B_PIN, OUTPUT);
  pinMode(KEY_0_PIN, INPUT_PULLUP);
  pinMode(KEY_1_PIN, INPUT_PULLUP);
  pinMode(A0, OUTPUT);
  pinMode(A1, OUTPUT);
  pinMode(A2, OUTPUT);
  pinMode(A3, OUTPUT);
  pinMode(A4, OUTPUT);
  pinMode(A5, OUTPUT);
  // TODO: set this baud rate in the Serial Monitor
  Serial.begin(115200);

  digitalWrite(JUDGE_LED_RED, HIGH);
  digitalWrite(JUDGE_LED_GREEN, HIGH);
  // print game/song params
  Serial.print("Setting global delay to (in ms): ");
  Serial.println(GLOBAL_DELAY_MS);
  Serial.print("Setting timing window to (in ms): ");
  Serial.println(TIMING_WINDOW_MS);
  Serial.print("Setting song's BPM to: ");
  Serial.println(BEATS_PER_MINUTE);
  Serial.print("Setting game's key number to: ");
  Serial.println(NUM_KEYS);
  Serial.print("Setting game's playfield rows number to: ");
  Serial.println(NUM_ROWS);
  Serial.print("Setting game's note speed to: ");
  Serial.println(NOTE_SPEED);
  Serial.print("Setting song's beat duration to (in ms): ");
  Serial.println(BEAT_DURATION_MS);
  Serial.print("Setting game's playfield draw distance to (in ms): ");
  Serial.println(PLAYFIELD_DRAW_DISTANCE_MS);
  Serial.print("Setting song's number of notes to: ");
  Serial.println(LAST_NOTE);

  // set judge LED to white to say it's waiting for the first input
  //setJudgeColor(RGB_WHITE);
  digitalWrite(A0, HIGH);
  digitalWrite(A1, HIGH);
  digitalWrite(A2, HIGH);
  digitalWrite(A3, HIGH);
  digitalWrite(A4, HIGH);
  digitalWrite(A5, HIGH);

  // Initialize display
  lc.shutdown(0,false);
  lc.setIntensity(0,8);
  lc.clearDisplay(0);
  
  // loop until we get any input
  // store the first key press to not judge a note because of it
  while (true) {
    bool starting = false;

    for (int i = 0; i < NUM_KEYS; ++i) {
      if (isKeyPressed(i)) {
        lastSeenKeys[i] = true;
        starting = true;
      }
    }

    if (starting) {
      break;
    }
  }

  // blink judge for a sec to say the game is starting
  setJudgeColor(RGB_BLACK);
  delay(500);
  setJudgeColor(RGB_GREEN);
  delay(1000);
  setJudgeColor(RGB_BLACK);

  // store play start millis
  gameStartMillis = millis();
}

void loop() {
  // clear playfield
  memset(playField, 0, sizeof(playField));

  // stop if the song is over
  if (nextNoteToJudge == LAST_NOTE) {
    return;
  }

  Serial.println("Song is playing");

  auto nowMs = millis() - gameStartMillis;

  Serial.print("Current song time (in ms): ");
  Serial.println(nowMs);

  // read pressed keys only once into an array
  bool pressedKeys[NUM_KEYS];
  for (int i = 0; i < NUM_KEYS; ++i) {
    pressedKeys[i] = isKeyPressed(i);
  }

  // DEBUG: dump pressed keys
  char playerKeys[NUM_KEYS + 1];
  for (int i = 0; i < NUM_KEYS; ++i) {
    if (pressedKeys[i]) {
      playerKeys[i] = '1';
    }
    else {
      playerKeys[i] = '0';
    }
  }
  playerKeys[NUM_KEYS] = '\0';
  Serial.print("Player keys: ");
  Serial.println(playerKeys);


  
  // judge new notes
  while (true) {
    Serial.print("Judging note ");
    Serial.println(nextNoteToJudge);

    // get the next note not yet judged
    auto& note = songNotes[nextNoteToJudge];
    auto adjustedNoteStartMs = note.startTime.toMs() + GLOBAL_DELAY_MS;

    Serial.print("Note's start time is ");
    Serial.println(adjustedNoteStartMs);

    // if it's ahead of the timing window...
    if (adjustedNoteStartMs > nowMs + TIMING_WINDOW_MS) {
      Serial.println("Note is ahead of the timing window");

      // any keypress here is a miss, but it doesn't count as a judged note
      // note: a keypress is a transition from key up to key down
      for (int i = 0; i < NUM_KEYS; ++i) {
        if (!lastSeenKeys[note.track] && pressedKeys[i]) {
          Serial.print("Found a MISS on key ");
          Serial.println(i);

          judgeMiss();
          break;
        }
      }

      // don't judge further notes
      break;
    }

    // if it's inside the timing window, check for keypress
    // note: a keypress is a transition from key up to key down
    // sign cast is needed because one side may be negative
    if ((int) adjustedNoteStartMs >= (int) nowMs - TIMING_WINDOW_MS) {
      Serial.println("Note is inside of the timing window");

      Serial.print("Checking note track: ");
      Serial.println(note.track);
      if (!lastSeenKeys[note.track] && pressedKeys[note.track]) {
        Serial.print("Found a HIT on key ");
        Serial.println(note.track);

        // store key now to prevent reprocessing it for another note
        lastSeenKeys[note.track] = true;
        judgeHit(note);
        nextNoteToJudge += 1;
      }

      // don't judge further notes
      break;
    }

    // if it's behind the timing window, it's a miss
    // sign cast is needed because one side may be negative
    if ((int) adjustedNoteStartMs < (int) nowMs - TIMING_WINDOW_MS) {
      Serial.println("Note is behind of the timing window -- MISS");

      judgeMiss();
      nextNoteToJudge += 1;
    }

    // stop if the song is over
    if (nextNoteToJudge == LAST_NOTE) {
      break;
    }
  }

  // draw remaining notes in the playfield
  int nextNoteToDraw = nextNoteToJudge;
  while (true) {
    Serial.print("Drawing note ");
    Serial.println(nextNoteToDraw);

  
    
    // get the next note not yet drawn
    auto& note = songNotes[nextNoteToDraw];
    auto adjustedNoteStartMs = note.startTime.toMs() + GLOBAL_DELAY_MS;

    // if it's ahead of the playfield, stop
    if (adjustedNoteStartMs > nowMs + PLAYFIELD_DRAW_DISTANCE_MS) {
      Serial.println("Note is not yet visible in the playfield");

      // don't draw further notes
      break;
    }

    // calculate this note's vertical position
    auto noteDelayMs = adjustedNoteStartMs - nowMs;
    Serial.print("Note's delay is ");
    Serial.println(noteDelayMs);
    int row = noteDelayMs * NOTE_SPEED / BEAT_DURATION_MS;
    Serial.print("Note's vertical position is ");
    Serial.println(row);

    // turn on the corresponding LED
    playField[row][note.track] = true;

    
    nextNoteToDraw += 1;


    
    // stop if the song is over
    if (nextNoteToDraw == LAST_NOTE) {
      break;
    }
  }

  // store current keys
  memcpy(lastSeenKeys, pressedKeys, NUM_KEYS);

  // DEBUG: print timestamp
  char timestamp[10];
  int nowMinutes = nowMs / (1000 * 60);
  nowMs = nowMs % (1000 * 60);
  int nowSeconds = nowMs / 1000;
  nowMs = nowMs % 1000;
  snprintf(timestamp, 9, "%02d:%02d.%03d", nowMinutes, nowSeconds, nowMs);
  Serial.println(timestamp);

  // print display
  int display;
  int points_cpy = points;
  Serial.print(points);
  for (int idx=0; idx<8; idx++)
  {
    display = points_cpy%10;
    points_cpy /= 10;
    lc.setDigit(0,idx,display, false);
  }
  

  
  // DEBUG: print playfield
  char playFieldString[NUM_ROWS * (NUM_KEYS + 2)]; // reserve space for \r\n
  for (int row = 0; row < NUM_ROWS; ++row) {
    for (int col = 0; col < NUM_KEYS; ++col) {
      if (playField[NUM_ROWS - row - 1][col]) {
        playFieldString[row * (NUM_KEYS + 2) + col] = '1';
        digitalWrite(playFieldPins[row][col], HIGH);
      }
      else {
        digitalWrite(playFieldPins[row][col], LOW);
        playFieldString[row * (NUM_KEYS + 2) + col] = '0';
      }

      playFieldString[row * (NUM_KEYS + 2) + NUM_KEYS] = '\r';
      playFieldString[row * (NUM_KEYS + 2) + NUM_KEYS + 1] = '\n';
    }
  }
  playFieldString[NUM_ROWS * (NUM_KEYS + 2) - 2] = '\0';
  Serial.println(playFieldString);
}
