package nguyenquyetchien;

import org.junit.jupiter.params.ParameterizedTest;
import org.junit.jupiter.params.provider.CsvSource;
import static org.junit.jupiter.api.Assertions.assertEquals;

public class Test13 {

    @ParameterizedTest(name = "Ngày {0}/{1}/{2} -> Thứ mong muốn (1=CN, 2=T2...): {3}")
    @CsvSource({
        "5, 4, 2020, 1",    // Đúng ví dụ đề bài: ngày 5/4/2020 là Chủ Nhật -> Trả về 1
        "6, 4, 2020, 2",    // Đúng ví dụ đề bài: ngày 6/4/2020 là Thứ Hai -> Trả về 2
        "28, 6, 2026, 1",   // Ngày hôm nay (28/06/2026) là Chủ Nhật -> Trả về 1
        "1, 1, 2026, 5"     // Ngày đầu năm 1/1/2026 là Thứ Năm -> Trả về 5
    })
    void testTinhThu(int ngay, int thang, int nam, int expectedDayOfWeek) {
        Advance7 advance = new Advance7();
        assertEquals(expectedDayOfWeek, advance.tinhThu(ngay, thang, nam));
    }
}