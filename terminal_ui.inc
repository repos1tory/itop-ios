@class ITopCanvas;
@protocol ITopCanvasDelegate <NSObject>
- (void)itopShowHelp;
- (void)itopChangeSort;
- (void)itopChangeFilter;
- (void)itopTogglePause;
- (void)itopRefresh;
@end

typedef struct {
    UIColor *background;
    UIColor *foreground;
    UIColor *dim;
    UIColor *green;
    UIColor *cyan;
    UIColor *blue;
    UIColor *yellow;
    UIColor *red;
    UIColor *selected;
    UIColor *selectedText;
} ITopTermTheme;

static ITopTermTheme ITopTheme(void) {
    static ITopTermTheme theme;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        // ANSI-like dark terminal palette, not the prior dashboard palette.
        theme.background = [UIColor colorWithRed:0.035 green:0.043 blue:0.055 alpha:1];
        theme.foreground = [UIColor colorWithRed:0.82 green:0.85 blue:0.89 alpha:1];
        theme.dim = [UIColor colorWithRed:0.43 green:0.49 blue:0.56 alpha:1];
        theme.green = [UIColor colorWithRed:0.63 green:0.85 blue:0.48 alpha:1];
        theme.cyan = [UIColor colorWithRed:0.35 green:0.78 blue:0.84 alpha:1];
        theme.blue = [UIColor colorWithRed:0.43 green:0.65 blue:0.91 alpha:1];
        theme.yellow = [UIColor colorWithRed:0.91 green:0.75 blue:0.43 alpha:1];
        theme.red = [UIColor colorWithRed:0.89 green:0.42 blue:0.45 alpha:1];
        theme.selected = [UIColor colorWithRed:0.20 green:0.27 blue:0.36 alpha:1];
        theme.selectedText = [UIColor colorWithRed:0.95 green:0.96 blue:0.98 alpha:1];
    });
    return theme;
}

static UIFont *ITopTermFont(CGFloat size, BOOL bold) {
    static NSMutableDictionary<NSString *, UIFont *> *cache;
    static dispatch_once_t once;
    dispatch_once(&once, ^{ cache = [NSMutableDictionary dictionary]; });
    NSString *key = [NSString stringWithFormat:@"%.1f/%d", size, bold];
    UIFont *font = cache[key];
    if (!font) {
        font = [UIFont fontWithName:bold ? @"Menlo-Bold" : @"Menlo" size:size];
        if (!font) font = [UIFont monospacedSystemFontOfSize:size weight:bold ? UIFontWeightBold : UIFontWeightRegular];
        cache[key] = font;
    }
    return font;
}

static NSMutableAttributedString *ITopTermBlank(NSUInteger columns, UIFont *font, UIColor *foreground) {
    NSString *spaces = [@"" stringByPaddingToLength:columns withString:@" " startingAtIndex:0];
    return [[NSMutableAttributedString alloc] initWithString:spaces attributes:@{
        NSFontAttributeName: font,
        NSForegroundColorAttributeName: foreground
    }];
}

static void ITopTermPut(NSMutableAttributedString *line, NSString *text, NSInteger column,
                        UIFont *font, UIColor *foreground, UIColor *background) {
    if (!line || !text || column < 0 || (NSUInteger)column >= line.length) return;
    NSUInteger count = MIN(text.length, line.length - (NSUInteger)column);
    if (!count) return;
    NSString *part = [text substringToIndex:count];
    NSRange range = NSMakeRange((NSUInteger)column, count);
    [line replaceCharactersInRange:range withString:part];
    NSMutableDictionary *attrs = [NSMutableDictionary dictionaryWithObjectsAndKeys:
        font, NSFontAttributeName, foreground, NSForegroundColorAttributeName, nil];
    if (background) attrs[NSBackgroundColorAttributeName] = background;
    [line addAttributes:attrs range:range];
}

static NSString *ITopTermRepeat(NSString *token, NSUInteger count) {
    if (!token.length || count == 0) return @"";
    NSMutableString *value = [NSMutableString stringWithCapacity:token.length * count];
    for (NSUInteger i = 0; i < count; i++) [value appendString:token];
    return value;
}

static NSString *ITopTermFixed(NSString *value, NSUInteger width, BOOL rightAligned) {
    if (!value) value = @"";
    if (value.length > width) value = [value substringToIndex:width];
    NSUInteger spaces = width - value.length;
    NSString *padding = ITopTermRepeat(@" ", spaces);
    return rightAligned ? [padding stringByAppendingString:value] : [value stringByAppendingString:padding];
}

static NSString *ITopTermBytes(uint64_t value) {
    if (value >= (1ull << 30)) return [NSString stringWithFormat:@"%.1fG", (double)value / (double)(1ull << 30)];
    if (value >= (1ull << 20)) return [NSString stringWithFormat:@"%.0fM", (double)value / (double)(1ull << 20)];
    if (value >= (1ull << 10)) return [NSString stringWithFormat:@"%.0fK", (double)value / 1024.0];
    return [NSString stringWithFormat:@"%lluB", (unsigned long long)value];
}

static NSString *ITopTermDuration(NSTimeInterval seconds) {
    NSUInteger total = (NSUInteger)MAX(0, seconds);
    NSUInteger days = total / 86400;
    NSUInteger hours = (total / 3600) % 24;
    NSUInteger minutes = (total / 60) % 60;
    if (days) return [NSString stringWithFormat:@"%lud %02lu:%02lu", (unsigned long)days,
                      (unsigned long)hours, (unsigned long)minutes];
    return [NSString stringWithFormat:@"%02lu:%02lu", (unsigned long)hours, (unsigned long)minutes];
}

static NSString *ITopTermCPUTime(uint64_t nanoseconds) {
    uint64_t centiseconds = nanoseconds / 10000000ull;
    uint64_t seconds = centiseconds / 100;
    return [NSString stringWithFormat:@"%02llu:%02llu.%02llu",
            (unsigned long long)(seconds / 60), (unsigned long long)(seconds % 60),
            (unsigned long long)(centiseconds % 100)];
}

static NSString *ITopEscapeKey(void) {
    unichar escape = 0x1B;
    return [NSString stringWithCharacters:&escape length:1];
}

static NSString *ITopTermUser(uid_t uid) {
    if (uid == (uid_t)-1) return @"?";
    if (uid == 0) return @"root";
    if (uid == 501) return @"mobile";
    return [NSString stringWithFormat:@"%u", uid];
}

static NSString *ITopTermBorder(NSUInteger width, NSString *title, BOOL top) {
    if (width < 2) return ITopTermRepeat(top ? @"─" : @"─", width);
    if (!top) return [NSString stringWithFormat:@"└%@┘", ITopTermRepeat(@"─", width - 2)];
    NSString *prefix = title.length ? [NSString stringWithFormat:@"┌─ %@ ", title] : @"┌";
    if (prefix.length > width - 1) prefix = [prefix substringToIndex:width - 1];
    NSMutableString *line = [NSMutableString stringWithString:prefix];
    while (line.length < width - 1) [line appendString:@"─"];
    [line appendString:@"┐"];
    return line;
}

static void ITopTermBoxSides(NSMutableAttributedString *line, NSUInteger column, NSUInteger width,
                             UIFont *font, UIColor *color) {
    ITopTermPut(line, @"│", (NSInteger)column, font, color, nil);
    if (width > 1) ITopTermPut(line, @"│", (NSInteger)(column + width - 1), font, color, nil);
}

// Log-scaled sparkline level for device network rates:
// level = clamp(round(log10(rate + 1) / log10(100000) * 7), 0..7).
// Each level step spans a rate factor of ~10^(5/7) ≈ 5.2x, so a few KB/s of
// idle chatter stays visible above the zero baseline while 100 KB/s
// (10^5 B/s) saturates the bar and anything faster clamps at the ceiling.
// The +1 inside log10 keeps rate 0 pinned at level 0 and avoids log(0).
static NSUInteger ITopNetLevelForRate(double rate) {
    if (!isfinite(rate)) return rate > 0.0 ? 7 : 0;
    if (rate <= 0.0) return 0;
    double level = round(log10(rate + 1.0) / log10(100000.0) * 7.0);
    if (level < 0.0) return 0;
    if (level > 7.0) return 7;
    return (NSUInteger)level;
}

// Ring window for the RX/TX sparklines, mirroring the _cpuHistory pattern:
// one value appended per second, trimmed to 64, pre-filled with zeros at init.
static void ITopNetHistoryAppend(NSMutableArray<NSNumber *> *history, double rate) {
    if (!history) return;
    [history addObject:@(MAX(0.0, rate))];
    if (history.count > 64) [history removeObjectAtIndex:0];
}

@interface ITopCanvas : UIView <UIKeyInput>
@property (nonatomic, weak) id<ITopCanvasDelegate> delegate;
@property (nonatomic, strong) ITopSnapshot *snapshot;
@property (nonatomic) double displayFPS;
@property (nonatomic) float batteryLevel;
@property (nonatomic) UIDeviceBatteryState batteryState;
@property (nonatomic, getter=isPaused) BOOL paused;
@property (nonatomic, copy) NSString *filterText;
@property (nonatomic, copy) NSString *sortKey;
@property (nonatomic) BOOL sortDescending;
@property (nonatomic) pid_t selectedPID;
@property (nonatomic) NSUInteger scrollOffset;
@property (nonatomic) CGFloat keyboardHeight;
@property (nonatomic) BOOL helpMode;
@property (nonatomic, copy) NSString *thermalState;
@property (nonatomic) uint64_t appAvailableMemory;
@property (nonatomic) double collectionMilliseconds;
@property (nonatomic) double wifiReceiveBytesPerSecond;
@property (nonatomic) double wifiSendBytesPerSecond;
@property (nonatomic) double cellularReceiveBytesPerSecond;
@property (nonatomic) double cellularSendBytesPerSecond;
@property (nonatomic) BOOL networkAvailable;
@property (nonatomic) BOOL energyAvailable;
@property (nonatomic) double ownerEnergyPowerMilliwatts;
- (void)moveSelectionBy:(NSInteger)delta;
- (void)beginSearch;
- (void)cancelSearch;
- (void)clearFilter;
- (BOOL)isSearching;
@end

@implementation ITopCanvas {
    NSArray<ITopProcess *> *_renderedRows;
    NSMutableArray<NSNumber *> *_cpuHistory;
    NSMutableArray<NSNumber *> *_netRxHistory;
    NSMutableArray<NSNumber *> *_netTxHistory;
    NSMutableString *_searchDraft;
    NSString *_searchOriginal;
    BOOL _searching;
    CGFloat _cellWidth;
    CGFloat _lineHeight;
    CGFloat _originY;
    NSUInteger _terminalColumns;
    NSUInteger _terminalRows;
    NSUInteger _processStartRow;
    NSUInteger _processDataStartRow;
    NSUInteger _rowCapacity;
    NSUInteger _footerRow;
    CGPoint _touchStart;
    CGPoint _touchLast;
    BOOL _didMove;
    NSInteger _helpCol;
    NSInteger _refreshCol;
    NSInteger _fpsCol;
    NSInteger _searchTagCol;
    NSUInteger _searchTagLen;
    NSUInteger _headingsRowActual;
    NSArray<NSDictionary<NSString *, id> *> *_headingZones;
}

- (instancetype)initWithFrame:(CGRect)frame {
    self = [super initWithFrame:frame];
    if (self) {
        self.opaque = YES;
        self.backgroundColor = ITopTheme().background;
        self.contentScaleFactor = UIScreen.mainScreen.scale;
        self.multipleTouchEnabled = NO;
        _filterText = @"";
        _sortKey = @"CPU%";
        _sortDescending = YES;
        _helpCol = NSNotFound;
        _refreshCol = NSNotFound;
        _fpsCol = NSNotFound;
        _searchTagCol = NSNotFound;
        _cpuHistory = [NSMutableArray arrayWithCapacity:64];
        for (NSUInteger i = 0; i < 64; i++) [_cpuHistory addObject:@0];
        _netRxHistory = [NSMutableArray arrayWithCapacity:64];
        _netTxHistory = [NSMutableArray arrayWithCapacity:64];
        for (NSUInteger i = 0; i < 64; i++) {
            [_netRxHistory addObject:@0];
            [_netTxHistory addObject:@0];
        }
        _batteryLevel = -1;
    }
    return self;
}

- (BOOL)isSearching { return _searching; }
- (BOOL)canBecomeFirstResponder { return YES; }
- (BOOL)hasText { return _searchDraft.length > 0; }
- (UIKeyboardType)keyboardType { return UIKeyboardTypeASCIICapable; }
- (UITextAutocapitalizationType)autocapitalizationType { return UITextAutocapitalizationTypeNone; }
- (UITextAutocorrectionType)autocorrectionType { return UITextAutocorrectionTypeNo; }
- (UIReturnKeyType)returnKeyType { return UIReturnKeyDone; }
- (UIKeyboardAppearance)keyboardAppearance { return UIKeyboardAppearanceDark; }

- (void)insertText:(NSString *)text {
    if (!_searching || !text.length) return;
    if ([text containsString:@"\n"] || [text containsString:@"\r"]) {
        _searching = NO;
        [self resignFirstResponder];
        [self setNeedsDisplay];
        return;
    }
    if ([text containsString:ITopEscapeKey()]) { [self cancelSearch]; return; }
    for (NSUInteger i = 0; i < text.length; i++) {
        unichar character = [text characterAtIndex:i];
        if (character >= 0x20 && character != 0x7f) [_searchDraft appendFormat:@"%C", character];
    }
    self.filterText = _searchDraft;
    self.scrollOffset = 0;
    [self setNeedsDisplay];
}

- (void)deleteBackward {
    if (!_searching || !_searchDraft.length) return;
    NSRange last = [_searchDraft rangeOfComposedCharacterSequenceAtIndex:_searchDraft.length - 1];
    [_searchDraft deleteCharactersInRange:last];
    self.filterText = _searchDraft;
    self.scrollOffset = 0;
    [self setNeedsDisplay];
}

- (void)beginSearch {
    _searchOriginal = [self.filterText copy] ?: @"";
    _searchDraft = [self.filterText mutableCopy] ?: [NSMutableString string];
    _searching = YES;
    self.helpMode = NO;
    [self becomeFirstResponder];
    [self setNeedsDisplay];
}

- (void)cancelSearch {
    if (!_searching) return;
    _searching = NO;
    self.filterText = _searchOriginal ?: @"";
    self.scrollOffset = 0;
    [self resignFirstResponder];
    [self setNeedsDisplay];
}

- (void)clearFilter {
    _searching = NO;
    self.filterText = @"";
    _searchDraft = [NSMutableString string];
    self.scrollOffset = 0;
    [self resignFirstResponder];
    [self setNeedsDisplay];
}

- (void)setSnapshot:(ITopSnapshot *)snapshot {
    _snapshot = snapshot;
    if (snapshot) {
        [_cpuHistory addObject:@(snapshot.cpu)];
        if (_cpuHistory.count > 64) [_cpuHistory removeObjectAtIndex:0];
        // Device totals (Wi-Fi + cellular) sampled once per second alongside the
        // CPU history; rates are zero while network sampling is unavailable.
        ITopNetHistoryAppend(_netRxHistory, self.wifiReceiveBytesPerSecond + self.cellularReceiveBytesPerSecond);
        ITopNetHistoryAppend(_netTxHistory, self.wifiSendBytesPerSecond + self.cellularSendBytesPerSecond);
    }
    [self setNeedsDisplay];
}

- (NSArray<ITopProcess *> *)visibleProcesses {
    NSArray<ITopProcess *> *source = self.snapshot.processes ?: @[];
    NSString *query = self.filterText.lowercaseString;
    NSMutableArray<ITopProcess *> *filtered = [NSMutableArray arrayWithCapacity:source.count];
    for (ITopProcess *process in source) {
        if (query.length && ![process.name.lowercaseString containsString:query] &&
            ![[NSString stringWithFormat:@"%d", process.pid] containsString:query]) continue;
        [filtered addObject:process];
    }
    NSString *key = self.sortKey ?: @"CPU%";
    BOOL descending = self.sortDescending;
    [filtered sortUsingComparator:^NSComparisonResult(ITopProcess *a, ITopProcess *b) {
        NSComparisonResult outcome = NSOrderedSame;
        if ([key isEqualToString:@"CPU%"]) {
            outcome = a.cpu < b.cpu ? NSOrderedAscending : (a.cpu > b.cpu ? NSOrderedDescending : NSOrderedSame);
        } else if ([key isEqualToString:@"RES"] || [key isEqualToString:@"MEM%"]) {
            outcome = a.residentBytes < b.residentBytes ? NSOrderedAscending :
                     (a.residentBytes > b.residentBytes ? NSOrderedDescending : NSOrderedSame);
        } else if ([key isEqualToString:@"VIRT"]) {
            outcome = a.virtualBytes < b.virtualBytes ? NSOrderedAscending :
                     (a.virtualBytes > b.virtualBytes ? NSOrderedDescending : NSOrderedSame);
        } else if ([key isEqualToString:@"TIME+"]) {
            outcome = a.cpuTimeNanoseconds < b.cpuTimeNanoseconds ? NSOrderedAscending :
                     (a.cpuTimeNanoseconds > b.cpuTimeNanoseconds ? NSOrderedDescending : NSOrderedSame);
        } else if ([key isEqualToString:@"PID"]) {
            outcome = a.pid < b.pid ? NSOrderedAscending : (a.pid > b.pid ? NSOrderedDescending : NSOrderedSame);
        } else if ([key isEqualToString:@"USER"]) {
            outcome = a.uid < b.uid ? NSOrderedAscending : (a.uid > b.uid ? NSOrderedDescending : NSOrderedSame);
        } else if ([key isEqualToString:@"PRI"]) {
            outcome = a.priority < b.priority ? NSOrderedAscending :
                     (a.priority > b.priority ? NSOrderedDescending : NSOrderedSame);
        } else if ([key isEqualToString:@"NI"]) {
            outcome = a.nice < b.nice ? NSOrderedAscending : (a.nice > b.nice ? NSOrderedDescending : NSOrderedSame);
        } else {
            outcome = [a.name localizedCaseInsensitiveCompare:b.name];
        }
        if (descending) {
            if (outcome == NSOrderedAscending) outcome = NSOrderedDescending;
            else if (outcome == NSOrderedDescending) outcome = NSOrderedAscending;
        }
        if (outcome == NSOrderedSame) return a.pid < b.pid ? NSOrderedAscending : NSOrderedDescending;
        return outcome;
    }];
    return filtered;
}

- (void)putBarOnLine:(NSMutableAttributedString *)line column:(NSUInteger)column width:(NSUInteger)width
              value:(double)value font:(UIFont *)font fillColor:(UIColor *)fillColor dimColor:(UIColor *)dimColor {
    NSUInteger filled = (NSUInteger)round((double)width * MAX(0, MIN(100, value)) / 100.0);
    ITopTermPut(line, ITopTermRepeat(@"█", filled), (NSInteger)column, font, fillColor, nil);
    ITopTermPut(line, ITopTermRepeat(@"░", width - filled), (NSInteger)(column + filled), font, dimColor, nil);
}

- (void)drawRect:(CGRect)dirtyRect {
    ITopTermTheme theme = ITopTheme();
    CGFloat width = self.bounds.size.width;
    CGFloat height = self.bounds.size.height - self.safeAreaInsets.bottom - self.keyboardHeight;
    BOOL compact = width < 620;
    CGFloat fontSize = compact ? 10.5 : 12.0;
    UIFont *font = ITopTermFont(fontSize, NO);
    UIFont *boldFont = ITopTermFont(fontSize, YES);
    NSDictionary *measure = @{ NSFontAttributeName: font };
    _cellWidth = MAX(1, [@"M" sizeWithAttributes:measure].width);
    _lineHeight = ceil(font.lineHeight + 1);
    _originY = 4;
    _terminalColumns = (NSUInteger)MAX(24, floor((width - 12) / _cellWidth));
    _terminalRows = (NSUInteger)MAX(12, floor((height - _originY - 2) / _lineHeight));
    _footerRow = _terminalRows - 1;
    CGContextRef context = UIGraphicsGetCurrentContext();
    CGContextSetFillColorWithColor(context, theme.background.CGColor);
    CGContextFillRect(context, self.bounds);

    NSMutableArray<NSMutableAttributedString *> *grid = [NSMutableArray arrayWithCapacity:_terminalRows];
    for (NSUInteger row = 0; row < _terminalRows; row++) [grid addObject:ITopTermBlank(_terminalColumns, font, theme.foreground)];
    NSUInteger columns = _terminalColumns;
    NSString *machine = self.snapshot.machine ?: @"device";
    NSString *osVersion = self.snapshot.osVersion ?: @"iOS";
    NSUInteger processCount = self.snapshot.processes.count;
    NSString *header = [NSString stringWithFormat:@"itop 1.1.0  │  iOS %@  │  %@", osVersion, machine];
    ITopTermPut(grid[0], @"itop", 0, boldFont, theme.green, nil);
    ITopTermPut(grid[0], [header substringFromIndex:4], 4, font, theme.foreground, nil);
    NSString *infoRight = [NSString stringWithFormat:@"%lu cores  %lu/%lu tasks  %@",
        (unsigned long)self.snapshot.coreUsage.count, (unsigned long)processCount,
        (unsigned long)self.snapshot.processCount, ITopTermDuration(self.snapshot.uptime)];
    NSString *fpsText = self.paused ? @"held" : [NSString stringWithFormat:@"%2.0f FPS", self.displayFPS];
    _fpsCol = (NSInteger)columns - (NSInteger)fpsText.length;
    _refreshCol = _fpsCol - 3;
    _helpCol = _refreshCol - 3;
    NSInteger infoColumn = _helpCol - (NSInteger)infoRight.length - 2;
    if (infoColumn > (NSInteger)header.length + 3 && infoColumn < _helpCol)
        ITopTermPut(grid[0], infoRight, infoColumn, boldFont, self.paused ? theme.yellow : theme.dim, nil);
    ITopTermPut(grid[0], @"?", _helpCol, boldFont, theme.cyan, nil);
    ITopTermPut(grid[0], @"R", _refreshCol, boldFont, theme.green, nil);
    ITopTermPut(grid[0], fpsText, _fpsCol, boldFont, self.paused ? theme.yellow : theme.green, nil);

    if (self.helpMode) {
        NSString *title = @"itop help";
        ITopTermPut(grid[2], ITopTermBorder(columns, title, YES), 0, boldFont, theme.cyan, nil);
        NSArray<NSString *> *help = @[
            @"  CPU, memory, battery and root filesystem stats refresh every second.",
            @"  Diagnostics: itop energy use, thermal state, network RX/TX rates,",
            @"  collection time, and this process's jetsam headroom (n/a if hidden by iOS).",
            @"  Selecting a row adds per-process energy plus fault/pagein/switch/syscall rates.",
            @"  Zone map: header '?', header 'R', FPS readout, Tasks-bar search tag, column titles.",
            @"  ?  (header)        Open or close this help page",
            @"  R  (header)        Re-sample right now",
            @"  FPS readout        Tap to hold/resume sampling; turns amber while held",
            @"  Column titles      Tap to sort; tap again to flip ▼/▲ direction",
            @"  search (Tasks bar) Open the terminal filter prompt; ✕ clears the filter",
            @"  ↑ / ↓ or swipe     Select and scroll processes",
            @"  Keyboard: S sort-cycle, F search, Space pause, R refresh, F1 help, Esc cancel",
            @"  Tap a process row to select it. No process is signalled or modified by itop."
        ];
        NSUInteger helpRow = 4;
        for (NSString *item in help) {
            if (helpRow >= _footerRow - 2) break;
            ITopTermPut(grid[helpRow++], item, 2, font, theme.foreground, nil);
        }
        ITopTermPut(grid[_footerRow - 1], @"Tap anywhere, or press Esc, to close this page", 0, boldFont, theme.cyan, nil);
        [self drawTerminalGrid:grid font:font originY:_originY];
        return;
    }

    NSUInteger cpuCount = self.snapshot.coreUsage.count;
    NSUInteger coreRows = MIN(cpuCount, (NSUInteger)(compact ? 4 : 8));
    NSUInteger panelHeight = compact ? 9 : coreRows + 5;
    NSUInteger leftWidth = compact ? columns : columns - columns / 2;
    NSUInteger rightWidth = compact ? columns : columns / 2;
    NSUInteger cpuX = 0;
    NSUInteger memoryX = compact ? 0 : leftWidth;
    NSUInteger memoryY = compact ? 1 + panelHeight : 1;
    _processStartRow = compact ? memoryY + panelHeight : 1 + panelHeight;

    NSUInteger actualCoreRows = MIN(coreRows, panelHeight - 4);
    ITopTermPut(grid[1], ITopTermBorder(leftWidth, @"CPU", YES), (NSInteger)cpuX, boldFont, theme.dim, nil);
    if (!compact) ITopTermPut(grid[1], ITopTermBorder(rightWidth, @"Mem", YES), (NSInteger)memoryX, boldFont, theme.dim, nil);
    ITopTermPut(grid[1], @"CPU", 3, boldFont, theme.cyan, nil);
    if (!compact) ITopTermPut(grid[1], @"Mem", (NSInteger)memoryX + 3, boldFont, theme.cyan, nil);

    for (NSUInteger core = 0; core < actualCoreRows; core++) {
        NSUInteger row = 2 + core;
        if (row >= _terminalRows) break;
        double usage = self.snapshot.coreUsage[core].doubleValue;
        ITopTermBoxSides(grid[row], cpuX, leftWidth, font, theme.dim);
        ITopTermPut(grid[row], [NSString stringWithFormat:@"%2lu", (unsigned long)core], 2, font, theme.cyan, nil);
        NSUInteger barWidth = leftWidth > 17 ? leftWidth - 17 : 1;
        NSUInteger barColumn = 6;
        UIColor *cpuColor = usage >= 85 ? theme.red : (usage >= 60 ? theme.yellow : theme.green);
        [self putBarOnLine:grid[row] column:barColumn width:barWidth value:usage font:font fillColor:cpuColor dimColor:theme.dim];
        ITopTermPut(grid[row], [NSString stringWithFormat:@"%5.1f%%", usage], (NSInteger)(leftWidth - 7), boldFont, cpuColor, nil);
    }
    NSUInteger cpuStatsRow = 2 + actualCoreRows;
    if (cpuStatsRow < _terminalRows) {
        ITopTermBoxSides(grid[cpuStatsRow], cpuX, leftWidth, font, theme.dim);
        ITopTermPut(grid[cpuStatsRow], [NSString stringWithFormat:@"avg %5.1f%%  %lu cores",
            self.snapshot.cpu, (unsigned long)cpuCount], 2, font, theme.foreground, nil);
        NSString *ownerEnergy = self.energyAvailable ?
            [NSString stringWithFormat:@"itop ENER %6.1fmW", self.ownerEnergyPowerMilliwatts] : @"itop ENER   n/a";
        ITopTermPut(grid[cpuStatsRow], ownerEnergy,
                    (NSInteger)(leftWidth - ownerEnergy.length - 2), boldFont,
                    self.energyAvailable ? (self.ownerEnergyPowerMilliwatts > 250 ? theme.red : theme.green) : theme.dim, nil);
    }
    NSUInteger diagRow = cpuStatsRow + 1;
    if (diagRow < _terminalRows) {
        ITopTermBoxSides(grid[diagRow], cpuX, leftWidth, font, theme.dim);
        NSString *netSummary;
        if (self.networkAvailable) {
            netSummary = [NSString stringWithFormat:@"Net W %@/%@  C %@/%@",
                ITopTermBytes((uint64_t)self.wifiReceiveBytesPerSecond),
                ITopTermBytes((uint64_t)self.wifiSendBytesPerSecond),
                ITopTermBytes((uint64_t)self.cellularReceiveBytesPerSecond),
                ITopTermBytes((uint64_t)self.cellularSendBytesPerSecond)];
        } else {
            netSummary = @"Net n/a";
        }
        NSString *thermal = self.thermalState ?: @"n/a";
        NSString *diagnostics = [NSString stringWithFormat:@"Therm %@  %@  Scan %.0fms",
            thermal, netSummary, self.collectionMilliseconds];
        UIColor *thermalColor = [thermal isEqualToString:@"CRITICAL"] || [thermal isEqualToString:@"SERIOUS"] ? theme.red :
            ([thermal isEqualToString:@"FAIR"] ? theme.yellow : theme.foreground);
        ITopTermPut(grid[diagRow], diagnostics, 2, font, thermalColor, nil);
    }
    NSUInteger graphRow = diagRow + 1;
    if (graphRow < _terminalRows) {
        ITopTermBoxSides(grid[graphRow], cpuX, leftWidth, font, theme.dim);
        NSUInteger chartWidth = leftWidth > 4 ? leftWidth - 4 : 1;
        NSString *levels = @"▁▂▃▄▅▆▇█";
        NSMutableString *chart = [NSMutableString stringWithCapacity:chartWidth];
        for (NSUInteger i = 0; i < chartWidth; i++) {
            NSUInteger historyIndex = chartWidth > 1 ? i * (_cpuHistory.count - 1) / (chartWidth - 1) : _cpuHistory.count - 1;
            double value = _cpuHistory[historyIndex].doubleValue;
            NSUInteger level = MIN(7, (NSUInteger)(MAX(0, MIN(99.9, value)) / 12.5));
            [chart appendString:[levels substringWithRange:NSMakeRange(level, 1)]];
        }
        ITopTermPut(grid[graphRow], chart, 2, font, theme.green, nil);
    }
    NSUInteger netRow = graphRow + 1;
    // Device RX/TX history row. The CPU panel interior spans rows 2 up to and
    // including panelHeight-1; the row is drawn only when one interior row is
    // still unclaimed after the CPU sparkline. The panel is never grown, so
    // saturated layouts (wide: height is exactly cores+5) skip it silently.
    if (netRow <= panelHeight - 1 && netRow < _terminalRows) {
        NSUInteger netInterior = leftWidth > 2 ? leftWidth - 2 : 1;
        NSUInteger graphColumns = netInterior > 12 ? MIN(16, (netInterior - 12) / 2) : 4;
        NSString *levels = @"▁▂▃▄▅▆▇█";
        NSMutableString *rxChart = [NSMutableString stringWithCapacity:graphColumns];
        NSMutableString *txChart = [NSMutableString stringWithCapacity:graphColumns];
        for (NSUInteger i = 0; i < graphColumns; i++) {
            NSUInteger rxIndex = graphColumns > 1 ? i * (_netRxHistory.count - 1) / (graphColumns - 1) : _netRxHistory.count - 1;
            NSUInteger txIndex = graphColumns > 1 ? i * (_netTxHistory.count - 1) / (graphColumns - 1) : _netTxHistory.count - 1;
            [rxChart appendString:[levels substringWithRange:NSMakeRange(ITopNetLevelForRate(_netRxHistory[rxIndex].doubleValue), 1)]];
            [txChart appendString:[levels substringWithRange:NSMakeRange(ITopNetLevelForRate(_netTxHistory[txIndex].doubleValue), 1)]];
        }
        NSUInteger txColumn = 7 + graphColumns;
        ITopTermPut(grid[netRow], @"RX ", 2, font, theme.green, nil);
        ITopTermPut(grid[netRow], rxChart, 5, font, theme.green, nil);
        ITopTermPut(grid[netRow], @"TX ", (NSInteger)txColumn, font, theme.cyan, nil);
        ITopTermPut(grid[netRow], txChart, (NSInteger)(txColumn + 3), font, theme.cyan, nil);
    }

    for (NSUInteger row = 2; row < 1 + panelHeight - 1 && row < _terminalRows; row++) {
        if (compact && row >= memoryY) continue;
        ITopTermBoxSides(grid[row], cpuX, leftWidth, font, theme.dim);
    }

    NSUInteger interiorWidth = rightWidth > 2 ? rightWidth - 2 : 1;
    NSUInteger memBarWidth = interiorWidth > 22 ? interiorWidth - 22 : 1;
    NSUInteger memBarColumn = memoryX + 7;
    uint64_t memoryTotal = self.snapshot.memoryTotal;
    double memoryPercent = memoryTotal ? (double)self.snapshot.memoryUsed * 100.0 / (double)memoryTotal : 0;
    UIColor *memoryColor = memoryPercent >= 90 ? theme.red : (memoryPercent >= 75 ? theme.yellow : theme.green);
    NSUInteger memRow = memoryY + 1;
    if (memRow < _terminalRows) {
        ITopTermBoxSides(grid[memRow], memoryX, rightWidth, font, theme.dim);
        ITopTermPut(grid[memRow], @"Mem", (NSInteger)memoryX + 2, font, theme.cyan, nil);
        [self putBarOnLine:grid[memRow] column:memBarColumn width:memBarWidth value:memoryPercent font:font fillColor:memoryColor dimColor:theme.dim];
        ITopTermPut(grid[memRow], [NSString stringWithFormat:@"%@/%@", ITopTermBytes(self.snapshot.memoryUsed), ITopTermBytes(memoryTotal)],
                    (NSInteger)(memoryX + rightWidth - 13), font, theme.foreground, nil);
    }
    memRow++;
    if (memRow < _terminalRows) {
        ITopTermBoxSides(grid[memRow], memoryX, rightWidth, font, theme.dim);
        ITopTermPut(grid[memRow], [NSString stringWithFormat:@"Wire %@  Comp %@",
            ITopTermFixed(ITopTermBytes(self.snapshot.memoryWired), 7, NO), ITopTermBytes(self.snapshot.memoryCompressed)],
            (NSInteger)memoryX + 2, font, theme.foreground, nil);
    }
    memRow++;
    if (memRow < _terminalRows) {
        ITopTermBoxSides(grid[memRow], memoryX, rightWidth, font, theme.dim);
        double rootPercent = self.snapshot.diskTotal ? (double)self.snapshot.diskUsed * 100.0 / (double)self.snapshot.diskTotal : 0;
        ITopTermPut(grid[memRow], @"Root", (NSInteger)memoryX + 2, font, theme.cyan, nil);
        [self putBarOnLine:grid[memRow] column:memBarColumn width:memBarWidth value:rootPercent font:font fillColor:theme.blue dimColor:theme.dim];
        ITopTermPut(grid[memRow], [NSString stringWithFormat:@"%@/%@", ITopTermBytes(self.snapshot.diskUsed), ITopTermBytes(self.snapshot.diskTotal)],
                    (NSInteger)(memoryX + rightWidth - 13), font, theme.foreground, nil);
    }
    memRow++;
    if (memRow < _terminalRows) {
        ITopTermBoxSides(grid[memRow], memoryX, rightWidth, font, theme.dim);
        NSString *battery = self.batteryLevel >= 0 ? [NSString stringWithFormat:@"Bat %.0f%%%@",
            self.batteryLevel * 100, self.batteryState == UIDeviceBatteryStateCharging ? @"+" : @""] : @"Bat --";
        NSString *uptime = [NSString stringWithFormat:@"Up %@", ITopTermDuration(self.snapshot.uptime)];
        ITopTermPut(grid[memRow], battery, (NSInteger)memoryX + 2, font,
                    self.batteryLevel >= 0 && self.batteryLevel < 0.15 ? theme.red : theme.yellow, nil);
        ITopTermPut(grid[memRow], uptime, (NSInteger)(memoryX + rightWidth - uptime.length - 3), font, theme.foreground, nil);
    }
    memRow++;
    if (memRow < _terminalRows) {
        ITopTermBoxSides(grid[memRow], memoryX, rightWidth, font, theme.dim);
        ITopTermPut(grid[memRow], @"Jtsm", (NSInteger)memoryX + 2, font, theme.cyan, nil);
        uint64_t headroom = self.appAvailableMemory;
        NSString *headroomText = headroom ? ITopTermBytes(headroom) : @"n/a";
        UIColor *headroomColor = headroom ? (headroom < 64ull << 20 ? theme.red :
            (headroom < 128ull << 20 ? theme.yellow : theme.green)) : theme.dim;
        ITopTermPut(grid[memRow], headroomText, (NSInteger)memoryX + 8, boldFont, headroomColor, nil);
        ITopTermPut(grid[memRow], @"headroom for this process", (NSInteger)memoryX + 8 + headroomText.length + 2,
                    font, theme.dim, nil);
    }

    if (compact) {
        ITopTermPut(grid[1 + panelHeight], ITopTermBorder(columns, @"Mem", YES), 0, boldFont, theme.dim, nil);
        ITopTermPut(grid[1 + panelHeight], @"Mem", 3, boldFont, theme.cyan, nil);
    }
    NSUInteger cpuBottom = 1 + panelHeight - 1;
    if (cpuBottom < _terminalRows) {
        ITopTermPut(grid[cpuBottom], ITopTermBorder(leftWidth, @"", NO), (NSInteger)cpuX, font, theme.dim, nil);
        if (!compact) ITopTermPut(grid[cpuBottom], ITopTermBorder(rightWidth, @"", NO), (NSInteger)memoryX, font, theme.dim, nil);
    }
    if (compact) {
        NSUInteger memoryBottom = memoryY + panelHeight - 1;
        if (memoryBottom < _terminalRows) ITopTermPut(grid[memoryBottom], ITopTermBorder(columns, @"", NO), 0, font, theme.dim, nil);
    }
    if (!compact) {
        for (NSUInteger row = 2; row < 1 + panelHeight - 1 && row < _terminalRows; row++)
            ITopTermBoxSides(grid[row], memoryX, rightWidth, font, theme.dim);
    }

    NSArray<ITopProcess *> *visible = [self visibleProcesses];
    _renderedRows = visible;
    _processStartRow = compact ? memoryY + panelHeight : 1 + panelHeight;
    _footerRow = _terminalRows - 1;
    if (_processStartRow + 4 >= _footerRow) _processStartRow = _footerRow > 7 ? _footerRow - 7 : 1;
    NSString *taskTitle = [NSString stringWithFormat:@"Tasks %lu/%lu",
        (unsigned long)visible.count, (unsigned long)self.snapshot.processCount];
    ITopTermPut(grid[_processStartRow], ITopTermBorder(columns, taskTitle, YES), 0, boldFont, theme.dim, nil);
    ITopTermPut(grid[_processStartRow], taskTitle, 3, boldFont, theme.cyan, nil);
    _searchTagCol = NSNotFound;
    NSString *searchBody = self.filterText.length ?
        [NSString stringWithFormat:@"search:%@", self.filterText] : @"search";
    NSArray<NSArray<id> *> *searchParts;
    if (self.filterText.length) {
        searchParts = @[
            @[@"search:", @"green"],
            @[self.filterText, @"amber"],
            @[@" ✕", @"red"]
        ];
    } else {
        searchParts = @[ @[@"search", @"green"] ];
    }
    NSUInteger searchTagLength = 0;
    for (NSArray<id> *part in searchParts) searchTagLength += [(NSString *)part[0] length];
    NSInteger searchTagColumn = (NSInteger)columns - (NSInteger)searchTagLength - 1;
    if (searchTagColumn > (NSInteger)taskTitle.length + 5) {
        _searchTagCol = searchTagColumn;
        _searchTagLen = searchTagLength;
        NSUInteger partColumn = (NSUInteger)searchTagColumn;
        for (NSArray<id> *part in searchParts) {
            NSString *text = part[0];
            UIColor *color = [part[1] isEqualToString:@"amber"] ? theme.yellow :
                ([part[1] isEqualToString:@"red"] ? theme.red : theme.green);
            ITopTermPut(grid[_processStartRow], text, (NSInteger)partColumn, boldFont, color, nil);
            partColumn += text.length;
        }
    }
    NSUInteger headingsRow = _processStartRow + 1;
    NSUInteger separatorRow = _processStartRow + 2;
    _processDataStartRow = _processStartRow + 3;
    NSUInteger statusRow = _footerRow;
    _rowCapacity = statusRow > _processDataStartRow ? statusRow - _processDataStartRow : 0;
    NSUInteger maximumOffset = visible.count > _rowCapacity ? visible.count - _rowCapacity : 0;
    self.scrollOffset = MIN(self.scrollOffset, maximumOffset);

    BOOL narrow = columns < 82;
    NSUInteger pidCol = 0, cpuCol, resCol, commandCol;
    if (narrow) { cpuCol = 8; resCol = 16; commandCol = 24; }
    else { cpuCol = 45; resCol = 33; commandCol = 69; }
    NSArray<NSDictionary<NSString *, id> *> *headingZones = @[
        @{ @"label": @"PID",     @"col": @0,  @"key": @"PID" },
        @{ @"label": @"USER",    @"col": @8,  @"key": @"USER" },
        @{ @"label": @"PRI",     @"col": @16, @"key": @"PRI" },
        @{ @"label": @"NI",      @"col": @20, @"key": @"NI" },
        @{ @"label": @"VIRT",    @"col": @24, @"key": @"VIRT" },
        @{ @"label": @"RES",     @"col": @32, @"key": @"RES" },
        @{ @"label": @"S",       @"col": @41, @"key": @"" },
        @{ @"label": @"CPU%",    @"col": @44, @"key": @"CPU%" },
        @{ @"label": @"MEM%",    @"col": @51, @"key": @"MEM%" },
        @{ @"label": @"TIME+",   @"col": @58, @"key": @"TIME+" },
        @{ @"label": @"Command", @"col": @69, @"key": @"Command" }
    ];
    if (narrow) {
        headingZones = @[
            @{ @"label": @"PID",     @"col": @0,          @"key": @"PID" },
            @{ @"label": @"CPU%",    @"col": @(cpuCol),   @"key": @"CPU%" },
            @{ @"label": @"RES",     @"col": @(resCol),   @"key": @"RES" },
            @{ @"label": @"Command", @"col": @(commandCol), @"key": @"Command" }
        ];
    }
    _headingZones = headingZones;
    _headingsRowActual = headingsRow;
    if (headingsRow < _terminalRows) {
        for (NSDictionary<NSString *, id> *zone in headingZones) {
            NSString *label = zone[@"label"];
            NSString *zoneKey = zone[@"key"];
            if (zoneKey.length == 0) continue;
            NSInteger labelColumn = [zone[@"col"] integerValue];
            BOOL active = [zoneKey isEqualToString:self.sortKey ?: @"CPU%"];
            ITopTermPut(grid[headingsRow], label, labelColumn, boldFont, active ? theme.green : theme.blue, nil);
            if (active) {
                ITopTermPut(grid[headingsRow], self.sortDescending ? @"▼" : @"▲",
                            labelColumn + (NSInteger)label.length, boldFont, theme.green, nil);
            }
        }
    }
    if (separatorRow < _terminalRows) ITopTermPut(grid[separatorRow], ITopTermRepeat(@"─", columns), 0, font, theme.dim, nil);

    NSUInteger end = MIN(visible.count, self.scrollOffset + _rowCapacity);
    for (NSUInteger index = self.scrollOffset; index < end; index++) {
        NSUInteger row = _processDataStartRow + index - self.scrollOffset;
        if (row >= statusRow) break;
        ITopProcess *process = visible[index];
        BOOL selected = process.pid == self.selectedPID;
        NSMutableAttributedString *line = grid[row];
        UIColor *cpuColor = process.cpu >= 70 ? theme.red : (process.cpu >= 20 ? theme.yellow : theme.green);
        if (selected) [line addAttribute:NSBackgroundColorAttributeName value:theme.selected range:NSMakeRange(0, line.length)];
        if (narrow) {
            ITopTermPut(line, ITopTermFixed([NSString stringWithFormat:@"%d", process.pid], 7, YES), pidCol, font, selected ? theme.selectedText : theme.dim, selected ? theme.selected : nil);
            ITopTermPut(line, @" ", pidCol + 7, font, selected ? theme.selectedText : theme.dim, selected ? theme.selected : nil);
            ITopTermPut(line, ITopTermFixed([NSString stringWithFormat:@"%.1f", process.cpu], 7, YES), cpuCol, font, selected ? theme.selectedText : cpuColor, selected ? theme.selected : nil);
            ITopTermPut(line, ITopTermFixed(ITopTermBytes(process.residentBytes), 7, YES), resCol, font, selected ? theme.selectedText : theme.foreground, selected ? theme.selected : nil);
        } else {
            double memoryPercentForProcess = memoryTotal ? (double)process.residentBytes * 100.0 / (double)memoryTotal : 0;
            ITopTermPut(line, ITopTermFixed([NSString stringWithFormat:@"%d", process.pid], 7, YES), 0, font, selected ? theme.selectedText : theme.dim, selected ? theme.selected : nil);
            ITopTermPut(line, ITopTermFixed(ITopTermUser(process.uid), 8, NO), 8, font, selected ? theme.selectedText : theme.cyan, selected ? theme.selected : nil);
            ITopTermPut(line, ITopTermFixed([NSString stringWithFormat:@"%d", process.priority], 4, YES), 16, font, selected ? theme.selectedText : theme.foreground, selected ? theme.selected : nil);
            ITopTermPut(line, ITopTermFixed([NSString stringWithFormat:@"%d", process.nice], 4, YES), 20, font, selected ? theme.selectedText : theme.foreground, selected ? theme.selected : nil);
            ITopTermPut(line, ITopTermFixed(ITopTermBytes(process.virtualBytes), 8, YES), 24, font, selected ? theme.selectedText : theme.foreground, selected ? theme.selected : nil);
            ITopTermPut(line, ITopTermFixed(ITopTermBytes(process.residentBytes), 8, YES), 32, font, selected ? theme.selectedText : theme.foreground, selected ? theme.selected : nil);
            ITopTermPut(line, process.runningThreads > 0 ? @"R" : @"S", 41, font, selected ? theme.selectedText : (process.runningThreads > 0 ? theme.green : theme.dim), selected ? theme.selected : nil);
            ITopTermPut(line, ITopTermFixed([NSString stringWithFormat:@"%.1f", process.cpu], 6, YES), 44, font, selected ? theme.selectedText : cpuColor, selected ? theme.selected : nil);
            UIColor *memColor = memoryPercentForProcess > 20 ? theme.red : theme.foreground;
            ITopTermPut(line, ITopTermFixed([NSString stringWithFormat:@"%.1f", memoryPercentForProcess], 6, YES), 51, font, selected ? theme.selectedText : memColor, selected ? theme.selected : nil);
            ITopTermPut(line, ITopTermFixed(ITopTermCPUTime(process.cpuTimeNanoseconds), 10, YES), 58, font, selected ? theme.selectedText : theme.dim, selected ? theme.selected : nil);
        }
        NSString *command = process.name ?: @"?";
        NSUInteger commandWidth = columns > commandCol ? columns - commandCol : 0;
        if (command.length > commandWidth) command = [command substringToIndex:commandWidth];
        ITopTermPut(line, command, (NSInteger)commandCol, selected ? boldFont : font,
                    selected ? theme.selectedText : theme.foreground, selected ? theme.selected : nil);
    }

    NSString *status;
    if (_searching) status = [NSString stringWithFormat:@"Search: %@_   Return: accept   Esc: cancel", _searchDraft ?: @""];
    else if (!self.snapshot.processAccess) status = @"ERROR: process-info access denied; check jailbreak signing entitlements";
    else if (visible.count == 0 && self.snapshot.processCount > 0) status = @"No readable processes; check PROC_PIDTASKALLINFO entitlement/ABI";
    else if (visible.count == 0) status = @"No matching processes";
    else if (self.selectedPID) {
        ITopProcess *selectedProcess = nil;
        for (ITopProcess *process in visible) if (process.pid == self.selectedPID) { selectedProcess = process; break; }
        if (selectedProcess) {
            NSString *processEnergy = selectedProcess.energyAvailable ?
                [NSString stringWithFormat:@"%.1fmW", selectedProcess.energyPowerMilliwatts] : @"n/a";
            status = [NSString stringWithFormat:@"PID %d  %@  THR %d/%d  CPU %.1f%%  ENER %@  FLT %.0f/s  PGI %.0f/s  CTX %.0f/s  SYS %.0f/s",
                selectedProcess.pid, selectedProcess.name, selectedProcess.runningThreads, selectedProcess.threads,
                selectedProcess.cpu, processEnergy, selectedProcess.faultsPerSecond, selectedProcess.pageinsPerSecond,
                selectedProcess.contextSwitchesPerSecond, selectedProcess.syscallsPerSecond];
        } else {
            status = [NSString stringWithFormat:@"Showing %lu-%lu of %lu",
                (unsigned long)(visible.count ? self.scrollOffset + 1 : 0), (unsigned long)end, (unsigned long)visible.count];
        }
    } else status = [NSString stringWithFormat:@"Showing %lu-%lu of %lu",
        (unsigned long)(visible.count ? self.scrollOffset + 1 : 0), (unsigned long)end, (unsigned long)visible.count];
    ITopTermPut(grid[statusRow], status, 0, font, _searching ? theme.yellow : theme.dim, nil);

    [self drawTerminalGrid:grid font:font originY:_originY];
}

- (void)drawTerminalGrid:(NSArray<NSMutableAttributedString *> *)grid font:(UIFont *)font originY:(CGFloat)originY {
    (void)font;
    CGFloat x = 6;
    for (NSUInteger row = 0; row < grid.count; row++) {
        [grid[row] drawAtPoint:CGPointMake(x, originY + row * _lineHeight)];
    }
}

- (void)moveSelectionBy:(NSInteger)delta {
    if (_renderedRows.count == 0) return;
    NSInteger current = NSNotFound;
    for (NSUInteger i = 0; i < _renderedRows.count; i++) {
        if (_renderedRows[i].pid == self.selectedPID) { current = (NSInteger)i; break; }
    }
    NSInteger next = current == NSNotFound ? 0 : MAX(0, MIN((NSInteger)_renderedRows.count - 1, current + delta));
    self.selectedPID = _renderedRows[(NSUInteger)next].pid;
    NSUInteger visibleIndex = (NSUInteger)next;
    if (visibleIndex < self.scrollOffset) self.scrollOffset = visibleIndex;
    else if (_rowCapacity && visibleIndex >= self.scrollOffset + _rowCapacity) self.scrollOffset = visibleIndex - _rowCapacity + 1;
    [self setNeedsDisplay];
}

- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
    _touchStart = [[touches anyObject] locationInView:self];
    _touchLast = _touchStart;
    _didMove = NO;
}

- (void)touchesMoved:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
    CGPoint point = [[touches anyObject] locationInView:self];
    CGFloat processTop = _originY + _processDataStartRow * _lineHeight;
    CGFloat footerTop = _originY + _footerRow * _lineHeight;
    if (fabs(point.y - _touchStart.y) > 10 && _touchStart.y >= processTop && _touchStart.y < footerTop) {
        _didMove = YES;
        NSInteger rows = (NSInteger)round((_touchLast.y - point.y) / MAX(1, _lineHeight));
        if (rows) {
            NSInteger limit = _renderedRows.count > _rowCapacity ? (NSInteger)(_renderedRows.count - _rowCapacity) : 0;
            self.scrollOffset = (NSUInteger)MAX(0, MIN(limit, (NSInteger)self.scrollOffset + rows));
            [self setNeedsDisplay];
            _touchLast = point;
        }
    }
}

- (void)touchesEnded:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
    if (_didMove) return;
    CGPoint point = [[touches anyObject] locationInView:self];
    NSUInteger column = (NSUInteger)MAX(0, (NSInteger)((point.x - 6) / MAX(1, _cellWidth)));
    NSUInteger row = (NSUInteger)MAX(0, floor((point.y - _originY) / MAX(1, _lineHeight)));
    if (self.helpMode) {
        self.helpMode = NO;
        [self setNeedsDisplay];
        return;
    }
    if (_searching) return;
    if (row == 0 && _helpCol != NSNotFound && _refreshCol != NSNotFound && _fpsCol != NSNotFound) {
        if (column >= (NSUInteger)MAX(0, _helpCol - 1) && column < (NSUInteger)_refreshCol - 1) {
            [self.delegate itopShowHelp];
        } else if (column >= (NSUInteger)_refreshCol - 1 && column < (NSUInteger)_fpsCol - 2) {
            [self.delegate itopRefresh];
        } else if (column >= (NSUInteger)_fpsCol - 2) {
            [self.delegate itopTogglePause];
        }
        return;
    }
    if (row == _processStartRow && _searchTagCol != NSNotFound) {
        if (column >= (NSUInteger)_searchTagCol && column < (NSUInteger)(_searchTagCol + _searchTagLen)) {
            if (self.filterText.length && column >= (NSUInteger)(_searchTagCol + _searchTagLen - 2)) {
                [self clearFilter];
            } else {
                [self.delegate itopChangeFilter];
            }
        }
        return;
    }
    if (row == _headingsRowActual) {
        for (NSDictionary<NSString *, id> *zone in _headingZones) {
            NSString *zoneKey = zone[@"key"];
            if (zoneKey.length == 0) continue;
            NSInteger labelColumn = [zone[@"col"] integerValue];
            NSUInteger labelLength = [zone[@"label"] length];
            if (column >= (NSUInteger)labelColumn && column <= (NSUInteger)(labelColumn + labelLength)) {
                if ([zoneKey isEqualToString:self.sortKey ?: @"CPU%"]) {
                    self.sortDescending = !self.sortDescending;
                } else {
                    self.sortKey = zoneKey;
                    self.sortDescending = [zoneKey isEqualToString:@"CPU%"] || [zoneKey isEqualToString:@"RES"] ||
                                          [zoneKey isEqualToString:@"VIRT"] || [zoneKey isEqualToString:@"MEM%"] ||
                                          [zoneKey isEqualToString:@"TIME+"];
                }
                self.scrollOffset = 0;
                [self setNeedsDisplay];
                return;
            }
        }
        return;
    }
    if (row >= _processDataStartRow && row < _processDataStartRow + _rowCapacity) {
        NSUInteger index = self.scrollOffset + row - _processDataStartRow;
        if (index < _renderedRows.count) {
            self.selectedPID = _renderedRows[index].pid;
            [self setNeedsDisplay];
        }
    }
}
@end
