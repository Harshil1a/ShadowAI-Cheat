-- ==========================================================================
-- SHADOW AI // CLOUD DATABASE SCHEMA (SUPABASE / POSTGRESQL)
-- 100% Free Tier Compatible (Up to 50,000 Monthly Active Users)
-- ==========================================================================

-- 1. USERS & PROFILES TABLE
-- Extends Supabase auth.users (Google SSO)
CREATE TABLE IF NOT EXISTS public.profiles (
    id UUID REFERENCES auth.users(id) ON DELETE CASCADE PRIMARY KEY,
    email TEXT UNIQUE NOT NULL,
    is_pro BOOLEAN DEFAULT FALSE,
    license_key TEXT,
    daily_queries_used INTEGER DEFAULT 0,
    last_query_date DATE DEFAULT CURRENT_DATE,
    created_at TIMESTAMP WITH TIME ZONE DEFAULT TIMEZONE('utc'::text, NOW()) NOT NULL
);

-- Enable Row Level Security (RLS)
ALTER TABLE public.profiles ENABLE ROW LEVEL SECURITY;

-- Policy: Users can read only their own profile
CREATE POLICY "Users can read own profile"
    ON public.profiles FOR SELECT
    USING (auth.uid() = id);

-- Policy: Users can update their own profile
CREATE POLICY "Users can update own profile"
    ON public.profiles FOR UPDATE
    USING (auth.uid() = id);

-- 2. LICENSES TABLE (Automated Checkout Webhooks from LemonSqueezy / Gumroad)
CREATE TABLE IF NOT EXISTS public.licenses (
    id UUID DEFAULT gen_random_uuid() PRIMARY KEY,
    license_key TEXT UNIQUE NOT NULL,
    customer_email TEXT NOT NULL,
    plan_tier TEXT DEFAULT 'PRO_MONTHLY',
    is_active BOOLEAN DEFAULT TRUE,
    activated_by_user_id UUID REFERENCES auth.users(id),
    created_at TIMESTAMP WITH TIME ZONE DEFAULT TIMEZONE('utc'::text, NOW()) NOT NULL
);

-- Enable RLS for licenses
ALTER TABLE public.licenses ENABLE ROW LEVEL SECURITY;

-- Policy: Anyone can validate a license key by key string
CREATE POLICY "Public key verification"
    ON public.licenses FOR SELECT
    USING (true);

-- 3. AUTO-USER CREATION TRIGGER (Fires automatically on Google Sign-In)
CREATE OR REPLACE FUNCTION public.handle_new_user()
RETURNS TRIGGER AS $$
BEGIN
    INSERT INTO public.profiles (id, email, is_pro, daily_queries_used, last_query_date)
    VALUES (
        NEW.id,
        NEW.email,
        FALSE,
        0,
        CURRENT_DATE
    );
    RETURN NEW;
END;
$$ LANGUAGE plpgsql SECURITY DEFINER;

-- Trigger execution
DROP TRIGGER IF EXISTS on_auth_user_created ON auth.users;
CREATE TRIGGER on_auth_user_created
    AFTER INSERT ON auth.users
    FOR EACH ROW EXECUTE PROCEDURE public.handle_new_user();

-- 4. FUNCTION: VERIFY & ATTACH LICENSE KEY
CREATE OR REPLACE FUNCTION public.activate_license(p_license_key TEXT)
RETURNS JSON AS $$
DECLARE
    v_lic RECORD;
    v_user_id UUID;
BEGIN
    v_user_id := auth.uid();
    
    SELECT * INTO v_lic FROM public.licenses
    WHERE license_key = UPPER(TRIM(p_license_key)) AND is_active = TRUE;
    
    IF NOT FOUND THEN
        RETURN json_build_object('success', false, 'message', 'Invalid or inactive license key.');
    END IF;
    
    -- Update Profile to Pro
    UPDATE public.profiles
    SET is_pro = TRUE,
        license_key = v_lic.license_key
    WHERE id = v_user_id;
    
    -- Mark License as attached
    UPDATE public.licenses
    SET activated_by_user_id = v_user_id
    WHERE id = v_lic.id;
    
    RETURN json_build_object('success', true, 'message', 'PRO license activated successfully!');
END;
$$ LANGUAGE plpgsql SECURITY DEFINER;
