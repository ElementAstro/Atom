#ifdef __APPLE__

#include "macos.hpp"
#include "linux.hpp" // For fallback POSIX functionality
#include <CoreFoundation/CoreFoundation.h>
#include <Foundation/Foundation.h>
#include <spdlog/spdlog.h>

namespace atom::system::locale::macos {

namespace {
    // Helper function to convert CFString to std::string
    std::string cfStringToString(CFStringRef cfStr) {
        if (!cfStr) return "";
        
        CFIndex length = CFStringGetLength(cfStr);
        CFIndex maxSize = CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8) + 1;
        std::string result(maxSize, '\0');
        
        if (CFStringGetCString(cfStr, &result[0], maxSize, kCFStringEncodingUTF8)) {
            result.resize(strlen(result.c_str()));
            return result;
        }
        return "";
    }
    
    // Helper function to create CFString from std::string
    CFStringRef stringToCFString(const std::string& str) {
        return CFStringCreateWithCString(kCFAllocatorDefault, str.c_str(), kCFStringEncodingUTF8);
    }
}

auto getCFLocaleInfo(const std::string& key, const std::string& locale) -> std::string {
    CFLocaleRef cfLocale;
    
    if (!locale.empty()) {
        CFStringRef localeIdentifier = stringToCFString(locale);
        cfLocale = CFLocaleCreate(kCFAllocatorDefault, localeIdentifier);
        CFRelease(localeIdentifier);
    } else {
        cfLocale = CFLocaleCopyCurrent();
    }
    
    if (!cfLocale) {
        return "";
    }
    
    CFStringRef keyRef = stringToCFString(key);
    CFTypeRef value = CFLocaleGetValue(cfLocale, keyRef);
    CFRelease(keyRef);
    
    std::string result;
    if (value) {
        if (CFGetTypeID(value) == CFStringGetTypeID()) {
            result = cfStringToString((CFStringRef)value);
        } else if (CFGetTypeID(value) == CFNumberGetTypeID()) {
            int intValue;
            if (CFNumberGetValue((CFNumberRef)value, kCFNumberIntType, &intValue)) {
                result = std::to_string(intValue);
            }
        }
    }
    
    CFRelease(cfLocale);
    return result;
}

auto getSystemLanguageInfo() -> LocaleInfo {
    spdlog::debug("Retrieving macOS system language information");
    LocaleInfo localeInfo;

    // Get current locale
    CFLocaleRef currentLocale = CFLocaleCopyCurrent();
    if (currentLocale) {
        CFStringRef localeIdentifier = CFLocaleGetIdentifier(currentLocale);
        localeInfo.localeName = cfStringToString(localeIdentifier);
        
        // Parse the locale identifier
        localeInfo = parseLocaleString(localeInfo.localeName);
        
        // Get detailed information using Core Foundation
        localeInfo.languageDisplayName = getCFLocaleInfo("kCFLocaleLanguageCode");
        localeInfo.countryDisplayName = getCFLocaleInfo("kCFLocaleCountryCode");
        localeInfo.currencySymbol = getCFLocaleInfo("kCFLocaleCurrencySymbol");
        localeInfo.currencyCode = getCFLocaleInfo("kCFLocaleCurrencyCode");
        localeInfo.decimalSymbol = getCFLocaleInfo("kCFLocaleDecimalSeparator");
        localeInfo.thousandSeparator = getCFLocaleInfo("kCFLocaleGroupingSeparator");
        localeInfo.characterEncoding = "UTF-8";
        
        CFRelease(currentLocale);
    }
    
    // Set additional properties
    localeInfo.isRTL = isRTLLocale(localeInfo.languageCode);
    localeInfo.measurementSystem = getMeasurementSystem(localeInfo.localeName);
    localeInfo.paperSize = getPaperSize(localeInfo.localeName);
    localeInfo.timeZone = getSystemTimeZone();
    localeInfo.firstDayOfWeek = getFirstDayOfWeek(localeInfo.localeName);
    localeInfo.weekendDays = getWeekendDays(localeInfo.localeName);
    
    // Get formatting information
    NSDateFormatter *dateFormatter = [[NSDateFormatter alloc] init];
    [dateFormatter setLocale:[NSLocale currentLocale]];
    [dateFormatter setDateStyle:NSDateFormatterShortStyle];
    localeInfo.dateFormat = std::string([[dateFormatter dateFormat] UTF8String]);
    
    [dateFormatter setDateStyle:NSDateFormatterNoStyle];
    [dateFormatter setTimeStyle:NSDateFormatterShortStyle];
    localeInfo.timeFormat = std::string([[dateFormatter dateFormat] UTF8String]);
    
    [dateFormatter release];

    spdlog::info("Successfully retrieved macOS locale information for: {}", localeInfo.localeName);
    return localeInfo;
}

auto getAvailableLocales() -> std::vector<std::string> {
    std::vector<std::string> locales;
    
    NSArray *availableLocaleIdentifiers = [NSLocale availableLocaleIdentifiers];
    for (NSString *identifier in availableLocaleIdentifiers) {
        locales.push_back(std::string([identifier UTF8String]));
    }
    
    spdlog::info("Found {} available locales on macOS", locales.size());
    return locales;
}

auto validateLocale(const std::string& locale) -> bool {
    if (locale.empty()) {
        return false;
    }
    
    NSString *localeIdentifier = [NSString stringWithUTF8String:locale.c_str()];
    NSLocale *testLocale = [[NSLocale alloc] initWithLocaleIdentifier:localeIdentifier];
    bool isValid = (testLocale != nil);
    [testLocale release];
    
    return isValid;
}

auto setSystemLocale(const std::string& locale) -> LocaleError {
    if (!validateLocale(locale)) {
        spdlog::error("Invalid locale: {}", locale);
        return LocaleError::InvalidLocale;
    }

    // On macOS, we can't directly set the system locale programmatically
    // We can only set the locale for the current process
    try {
        // Fallback to POSIX setlocale
        return linux_impl::setSystemLocale(locale);
    } catch (const std::exception& e) {
        spdlog::error("Exception while setting macOS locale {}: {}", locale, e.what());
        return LocaleError::SystemError;
    }
}

auto getDefaultLocale() -> std::string {
    spdlog::debug("Getting macOS default locale");
    
    NSLocale *currentLocale = [NSLocale currentLocale];
    NSString *localeIdentifier = [currentLocale localeIdentifier];
    return std::string([localeIdentifier UTF8String]);
}

auto getPreferredLanguages() -> std::vector<std::string> {
    std::vector<std::string> languages;
    
    NSArray *preferredLanguages = [NSLocale preferredLanguages];
    for (NSString *language in preferredLanguages) {
        languages.push_back(std::string([language UTF8String]));
    }
    
    return languages;
}

auto getCurrentLocale() -> std::string {
    return getDefaultLocale();
}

auto isRTLLocale(const std::string& locale) -> bool {
    NSString *localeIdentifier = [NSString stringWithUTF8String:locale.c_str()];
    NSLocale *testLocale = [[NSLocale alloc] initWithLocaleIdentifier:localeIdentifier];
    
    NSLocaleLanguageDirection direction = [NSLocale characterDirectionForLanguage:[testLocale objectForKey:NSLocaleLanguageCode]];
    bool isRTL = (direction == NSLocaleLanguageDirectionRightToLeft);
    
    [testLocale release];
    return isRTL;
}

auto getCurrencyInfo(const std::string& locale) -> std::pair<std::string, std::string> {
    std::string symbol = getCFLocaleInfo("kCFLocaleCurrencySymbol", locale);
    std::string code = getCFLocaleInfo("kCFLocaleCurrencyCode", locale);
    return {symbol, code};
}

auto getMeasurementSystem(const std::string& locale) -> MeasurementSystem {
    NSString *localeIdentifier = [NSString stringWithUTF8String:locale.c_str()];
    NSLocale *testLocale = [[NSLocale alloc] initWithLocaleIdentifier:localeIdentifier];
    
    NSNumber *usesMetric = [testLocale objectForKey:NSLocaleUsesMetricSystem];
    bool isMetric = [usesMetric boolValue];
    
    [testLocale release];
    return isMetric ? MeasurementSystem::Metric : MeasurementSystem::US;
}

auto getPaperSize(const std::string& locale) -> PaperSize {
    // macOS doesn't provide direct API for paper size, use region-based logic
    LocaleInfo info = parseLocaleString(locale);
    if (info.countryCode == "US" || info.countryCode == "CA" || info.countryCode == "MX") {
        return PaperSize::Letter;
    }
    return PaperSize::A4;
}

auto getFirstDayOfWeek(const std::string& locale) -> std::string {
    NSString *localeIdentifier = [NSString stringWithUTF8String:locale.c_str()];
    NSLocale *testLocale = [[NSLocale alloc] initWithLocaleIdentifier:localeIdentifier];
    
    NSCalendar *calendar = [[NSCalendar alloc] initWithCalendarIdentifier:NSCalendarIdentifierGregorian];
    [calendar setLocale:testLocale];
    
    NSInteger firstWeekday = [calendar firstWeekday];
    const std::vector<std::string> days = {
        "", "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"
    };
    
    std::string result = (firstWeekday >= 1 && firstWeekday <= 7) ? days[firstWeekday] : "Monday";
    
    [calendar release];
    [testLocale release];
    return result;
}

auto getWeekendDays(const std::string& locale) -> std::vector<std::string> {
    // Most regions have Saturday and Sunday as weekend
    return {"Saturday", "Sunday"};
}

auto getSystemTimeZone() -> std::string {
    NSTimeZone *timeZone = [NSTimeZone systemTimeZone];
    return std::string([[timeZone name] UTF8String]);
}

auto formatNumber(double number, const std::string& locale) -> std::string {
    NSString *localeIdentifier = [NSString stringWithUTF8String:locale.c_str()];
    NSLocale *testLocale = [[NSLocale alloc] initWithLocaleIdentifier:localeIdentifier];
    
    NSNumberFormatter *formatter = [[NSNumberFormatter alloc] init];
    [formatter setLocale:testLocale];
    [formatter setNumberStyle:NSNumberFormatterDecimalStyle];
    
    NSNumber *number_obj = [NSNumber numberWithDouble:number];
    NSString *formatted = [formatter stringFromNumber:number_obj];
    std::string result = std::string([formatted UTF8String]);
    
    [formatter release];
    [testLocale release];
    return result;
}

auto formatCurrency(double amount, const std::string& locale) -> std::string {
    NSString *localeIdentifier = [NSString stringWithUTF8String:locale.c_str()];
    NSLocale *testLocale = [[NSLocale alloc] initWithLocaleIdentifier:localeIdentifier];
    
    NSNumberFormatter *formatter = [[NSNumberFormatter alloc] init];
    [formatter setLocale:testLocale];
    [formatter setNumberStyle:NSNumberFormatterCurrencyStyle];
    
    NSNumber *amount_obj = [NSNumber numberWithDouble:amount];
    NSString *formatted = [formatter stringFromNumber:amount_obj];
    std::string result = std::string([formatted UTF8String]);
    
    [formatter release];
    [testLocale release];
    return result;
}

auto formatDate(const std::chrono::system_clock::time_point& timestamp, 
                const std::string& locale, 
                const std::string& format) -> std::string {
    NSString *localeIdentifier = [NSString stringWithUTF8String:locale.c_str()];
    NSLocale *testLocale = [[NSLocale alloc] initWithLocaleIdentifier:localeIdentifier];
    
    NSDateFormatter *formatter = [[NSDateFormatter alloc] init];
    [formatter setLocale:testLocale];
    
    if (!format.empty()) {
        NSString *formatString = [NSString stringWithUTF8String:format.c_str()];
        [formatter setDateFormat:formatString];
    } else {
        [formatter setDateStyle:NSDateFormatterShortStyle];
        [formatter setTimeStyle:NSDateFormatterNoStyle];
    }
    
    auto time_t = std::chrono::system_clock::to_time_t(timestamp);
    NSDate *date = [NSDate dateWithTimeIntervalSince1970:time_t];
    NSString *formatted = [formatter stringFromDate:date];
    std::string result = std::string([formatted UTF8String]);
    
    [formatter release];
    [testLocale release];
    return result;
}

auto formatTime(const std::chrono::system_clock::time_point& timestamp, 
                const std::string& locale, 
                const std::string& format) -> std::string {
    NSString *localeIdentifier = [NSString stringWithUTF8String:locale.c_str()];
    NSLocale *testLocale = [[NSLocale alloc] initWithLocaleIdentifier:localeIdentifier];
    
    NSDateFormatter *formatter = [[NSDateFormatter alloc] init];
    [formatter setLocale:testLocale];
    
    if (!format.empty()) {
        NSString *formatString = [NSString stringWithUTF8String:format.c_str()];
        [formatter setDateFormat:formatString];
    } else {
        [formatter setDateStyle:NSDateFormatterNoStyle];
        [formatter setTimeStyle:NSDateFormatterShortStyle];
    }
    
    auto time_t = std::chrono::system_clock::to_time_t(timestamp);
    NSDate *date = [NSDate dateWithTimeIntervalSince1970:time_t];
    NSString *formatted = [formatter stringFromDate:date];
    std::string result = std::string([formatted UTF8String]);
    
    [formatter release];
    [testLocale release];
    return result;
}

auto getSystemRegion() -> std::string {
    NSLocale *currentLocale = [NSLocale currentLocale];
    NSString *countryCode = [currentLocale objectForKey:NSLocaleCountryCode];
    return std::string([countryCode UTF8String]);
}

auto getSystemLanguage() -> std::string {
    NSLocale *currentLocale = [NSLocale currentLocale];
    NSString *languageCode = [currentLocale objectForKey:NSLocaleLanguageCode];
    return std::string([languageCode UTF8String]);
}

auto getCalendarIdentifier(const std::string& locale) -> std::string {
    NSString *localeIdentifier = [NSString stringWithUTF8String:locale.c_str()];
    NSLocale *testLocale = [[NSLocale alloc] initWithLocaleIdentifier:localeIdentifier];
    
    NSCalendar *calendar = [NSCalendar currentCalendar];
    [calendar setLocale:testLocale];
    
    NSString *identifier = [calendar calendarIdentifier];
    std::string result = std::string([identifier UTF8String]);
    
    [testLocale release];
    return result;
}

auto getNumberFormattingStyle(const std::string& locale) -> std::string {
    return getCFLocaleInfo("kCFLocaleDecimalSeparator", locale) + " / " + 
           getCFLocaleInfo("kCFLocaleGroupingSeparator", locale);
}

auto getDateFormattingStyle(const std::string& locale) -> std::string {
    NSString *localeIdentifier = [NSString stringWithUTF8String:locale.c_str()];
    NSLocale *testLocale = [[NSLocale alloc] initWithLocaleIdentifier:localeIdentifier];
    
    NSDateFormatter *formatter = [[NSDateFormatter alloc] init];
    [formatter setLocale:testLocale];
    [formatter setDateStyle:NSDateFormatterShortStyle];
    
    NSString *format = [formatter dateFormat];
    std::string result = std::string([format UTF8String]);
    
    [formatter release];
    [testLocale release];
    return result;
}

auto getTimeFormattingStyle(const std::string& locale) -> std::string {
    NSString *localeIdentifier = [NSString stringWithUTF8String:locale.c_str()];
    NSLocale *testLocale = [[NSLocale alloc] initWithLocaleIdentifier:localeIdentifier];
    
    NSDateFormatter *formatter = [[NSDateFormatter alloc] init];
    [formatter setLocale:testLocale];
    [formatter setTimeStyle:NSDateFormatterShortStyle];
    
    NSString *format = [formatter dateFormat];
    std::string result = std::string([format UTF8String]);
    
    [formatter release];
    [testLocale release];
    return result;
}

auto uses24HourFormat() -> bool {
    NSDateFormatter *formatter = [[NSDateFormatter alloc] init];
    [formatter setLocale:[NSLocale currentLocale]];
    [formatter setTimeStyle:NSDateFormatterShortStyle];
    
    NSString *format = [formatter dateFormat];
    bool uses24Hour = ([format rangeOfString:@"H"].location != NSNotFound);
    
    [formatter release];
    return uses24Hour;
}

auto getDecimalSeparator(const std::string& locale) -> std::string {
    return getCFLocaleInfo("kCFLocaleDecimalSeparator", locale);
}

auto getThousandsSeparator(const std::string& locale) -> std::string {
    return getCFLocaleInfo("kCFLocaleGroupingSeparator", locale);
}

auto getListSeparator(const std::string& locale) -> std::string {
    // macOS doesn't provide direct API for list separator, use common defaults
    LocaleInfo info = parseLocaleString(locale);
    if (info.countryCode == "US" || info.countryCode == "GB") {
        return ",";
    }
    return ";";
}

auto getQuotationMarks(const std::string& locale) -> std::pair<std::string, std::string> {
    std::string begin = getCFLocaleInfo("kCFLocaleQuotationBeginDelimiterKey", locale);
    std::string end = getCFLocaleInfo("kCFLocaleQuotationEndDelimiterKey", locale);
    if (begin.empty()) begin = "\"";
    if (end.empty()) end = "\"";
    return {begin, end};
}

auto getExemplarCharacters(const std::string& locale) -> std::string {
    // This would require ICU or similar library for full implementation
    // Return basic ASCII for now
    return "abcdefghijklmnopqrstuvwxyz";
}

} // namespace atom::system::locale::macos

#endif // __APPLE__
