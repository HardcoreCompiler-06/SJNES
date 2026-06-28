package nguyenquyetchien;

import org.junit.jupiter.params.ParameterizedTest;
import org.junit.jupiter.params.provider.CsvSource;
import static org.junit.jupiter.api.Assertions.assertEquals;

public class Test15 {

    @ParameterizedTest(name = "Chuỗi đầu vào: ''{0}'' -> Chuỗi đảo ngược mong muốn: ''{1}''")
    @CsvSource({
        "hello, olleh",      // Chuỗi thông thường
        "a, a",              // Chuỗi chỉ có 1 ký tự
        "radar, radar",      // Chuỗi đối xứng (Palindrome)
        "'hello world', 'dlrow olleh'", // Chuỗi có chứa khoảng trắng
        "'', ''"             // Chuỗi rỗng (vòng lặp không chạy, trả về chuỗi rỗng)
    })
    void testReverseString(String input, String expected) {
        // Vì tham số truyền vào CsvSource có thể nhận giá trị rỗng, ta xử lý chuỗi rỗng chuẩn chỉnh
        String actualInput = (input == null) ? "" : input;
        String actualExpected = (expected == null) ? "" : expected;
        
        assertEquals(actualExpected, StringReversal.reverseString(actualInput));
    }
}