package nguyenducan;

public class LoginService {
    private static final String USERNAME = "user";
    private static final String PASSWORD = "password";

    public static boolean login(String username, String password) {
        return username.equals(USERNAME) && password.equals(PASSWORD);
    }
}