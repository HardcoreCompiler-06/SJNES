package nguyenquyetchien;

import org.junit.jupiter.params.ParameterizedTest;
import org.junit.jupiter.params.provider.CsvSource;
import static org.junit.jupiter.api.Assertions.assertEquals;

public class Test10 {

    @ParameterizedTest(name = "n = {0} -> Kết quả kiểm tra số nguyên tố: {1}")
    @CsvSource({
        "1, false",    // Biên n < 2 (Số 1 không phải số nguyên tố)
        "-5, false",   // Số âm n < 2
        "2, true",     // Số nguyên tố nhỏ nhất (vòng lặp không chạy vì 2 <= 1 là sai)
        "3, true",     // Số nguyên tố lẻ (vòng lặp không chạy vì 2 <= 1 là sai)
        "4, false",    // Hợp số, chia hết cho 2 (vòng lặp chạy i=2, 4 % 2 == 0)
        "9, false",    // Số chính phương lẻ (vòng lặp chạy đến i=3, 9 % 3 == 0)
        "17, true"     // Số nguyên tố lớn hơn (vòng lặp chạy hết không tìm thấy ước)
    })
    void testIsPrimeNumber(int n, boolean expected) {
        Advance4 advance = new Advance4();
        assertEquals(expected, advance.isPrimeNumber(n));
    }
}