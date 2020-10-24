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

  unsigned int ToMs() {
    return minutes * 60 * 1000 + seconds * 1000 + ms;
  }
};

class Note {
public:
  Time startTime = Time(0, 0, 0);
  byte track;
  unsigned int frequency;
  unsigned long durationMs;

  Note(byte track, Time startTime, unsigned int frequency, unsigned long durationMs) {
    this->track = track;
    this->startTime = startTime;
    this->frequency = frequency;
    this->durationMs = durationMs;
  }
};

Note songNotes[] = {
  Note(0, Time(0, 0, 0), 1500, 500),
  Note(0, Time(0, 1, 0), 500, 250),
  Note(0, Time(0, 2, 0), 1500, 500),
  Note(0, Time(0, 3, 0), 500, 250)
};

unsigned int nextNoteToJudge = 0;
unsigned int gameStartMillis = 0;

// game/song params
const int GLOBAL_DELAY_MS = 0; // used to adjust both song and notes together
const int GLOBAL_OFFSET_MS = 0; // used to offset notes in relation to song
const int TIMING_WINDOW_MS = 60; // how many ms can the player hit the note before or after the note's time
const int BEATS_PER_MINUTE = 120; // song's BPM
const int NUM_KEYS = 2; // number of player input keys
const int NUM_ROWS = 10; // number of rows in the playfield
const int NOTE_SPEED = 2; // number of rows of separation between consecutive notes
const float BEAT_DURATION = 60.0 / BEATS_PER_MINUTE;
const int PLAYFIELD_DRAW_DISTANCE_MS = (float) NUM_ROWS / NOTE_SPEED * BEAT_DURATION;
const unsigned int LAST_NOTE = sizeof(songNotes) / sizeof(songNotes[0]);

// pins
const int BUZZER_PIN = 3; // PWM pin for the buzzer
const int JUDGE_RGB_LED_R_PIN = 11; // red pin for an RGB LED (used to tell the player if he hit or missed the note)
const int JUDGE_RGB_LED_G_PIN = 10; // green pin for an RGB LED
const int JUDGE_RGB_LED_B_PIN = 9; // blue pin for an RGB LED
const int KEY_0_PIN = A0; // pin for the first game button (leftmost)
const int KEY_1_PIN = A1; // pin for the second game button

// colors
const byte RGB_BLACK[] = {0, 0, 0};
const byte RGB_WHITE[] = {255, 255, 255};
const byte RGB_RED[] = {255, 0, 0};
const byte RGB_GREEN[] = {0, 255, 0};
const byte RGB_BLUE[] = {0, 0, 255};

// input state
bool lastSeenKeys[NUM_KEYS] = {0};

// playfield state
bool playField[NUM_ROWS][5] = {0};

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
  setJudgeColor(RGB_RED);
  noTone(BUZZER_PIN);
}

void judgeHit(const Note& note) {
  // TODO: blink and then reset?
  setJudgeColor(RGB_GREEN);
  tone(note.frequency, note.durationMs);
}

void setup() {
  Serial.begin(9600);

  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(JUDGE_RGB_LED_R_PIN, OUTPUT);
  pinMode(JUDGE_RGB_LED_G_PIN, OUTPUT);
  pinMode(JUDGE_RGB_LED_B_PIN, OUTPUT);
  pinMode(KEY_0_PIN, INPUT);
  pinMode(KEY_1_PIN, INPUT);

  // set judge LED to white to say it's waiting for the first input
  setJudgeColor(RGB_WHITE);

  // loop until we get any input
  while (true) {
    if (digitalRead(KEY_0_PIN) == LOW
        || digitalRead(KEY_1_PIN) == LOW) {
      break;
    }
  }

  // blink judge for a sec to say the game is starting
  setJudgeColor(RGB_BLACK);
  delay(500);
  setJudgeColor(RGB_GREEN);
  delay(1000);
  setJudgeColor(RGB_BLACK);
  delay(1000);

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

  auto nowMs = millis() - gameStartMillis;

  // judge new notes
  while (true) {
    // get the next note not yet judged
    auto& note = songNotes[nextNoteToJudge];

    // if it's ahead of the timing window...
    if (note.startTime.ToMs() > nowMs + TIMING_WINDOW_MS) {
      // any keypress here is a miss
      // note: a keypress is a transition from key up to key down
      for (int i = 0; i < NUM_KEYS; ++i) {
        if (!lastSeenKeys[note.track] && isKeyPressed(i)) {
          judgeMiss();
          break;
        }
      }

      // don't judge further notes
      break;
    }

    // if it's inside the timing window, check for keypress
    // note: a keypress is a transition from key up to key down
    if (!lastSeenKeys[note.track] && isKeyPressed(note.track)) {
      // store key now to prevent reprocessing it for another note
      lastSeenKeys[note.track] = true;
      judgeHit(note);
    }

    // if it's behind the timing window, it's a miss
    if (note.startTime.ToMs() < nowMs - TIMING_WINDOW_MS) {
      judgeMiss();
    }

    nextNoteToJudge += 1;
  }

  // draw remaining notes in the playfield
  int nextNoteToDraw = nextNoteToJudge;
  while (true) {
    // get the next note not yet drawn
    auto& note = songNotes[nextNoteToDraw];

    // if it's ahead of the playfield, stop
    if (note.startTime.ToMs() > nowMs + PLAYFIELD_DRAW_DISTANCE_MS) {
      break;
    }

    // calculate this note's position
    // TODO
    int row = 1;

    // turn on the corresponding LED
    playField[row][note.track] = true;
  }

  // store current keys
  for (int i = 0; i < NUM_KEYS; ++i) {
    lastSeenKeys[i] = isKeyPressed(i);
  }

  // DEBUG: print resulting playfield
  // TODO: use a terminal emulator instead?
  return;
  for (int col = 0; col < NUM_KEYS; ++col) {
    Serial.print("-");
  }
  Serial.print("\r\n");
  for (int row = NUM_ROWS - 1; row > 0; --row) {
    // print row
    for (int col = 0; col < NUM_KEYS; ++col) {
      Serial.print((int) playField[row][col]);

      if (col == NUM_KEYS - 1) {
        Serial.print("\r\n");
      }
    }
  }
  for (int col = 0; col < NUM_KEYS; ++col) {
    Serial.print("-");
  }
  Serial.print("\r\n\r\n\r\n");
}
