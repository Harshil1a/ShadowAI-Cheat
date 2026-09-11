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

-- Add HWID column for device locking
ALTER TABLE public.licenses ADD COLUMN IF NOT EXISTS bound_hwid TEXT;
ALTER TABLE public.licenses ADD COLUMN IF NOT EXISTS last_used_at TIMESTAMP WITH TIME ZONE;

-- Allow insert & update policies for public/admin
DROP POLICY IF EXISTS "Public key insert" ON public.licenses;
CREATE POLICY "Public key insert" ON public.licenses FOR INSERT WITH CHECK (true);

DROP POLICY IF EXISTS "Public key update" ON public.licenses;
CREATE POLICY "Public key update" ON public.licenses FOR UPDATE USING (true);

DROP POLICY IF EXISTS "Public key delete" ON public.licenses;
CREATE POLICY "Public key delete" ON public.licenses FOR DELETE USING (true);

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

-- 5. FUNCTION: HARDWARE-LOCKED LICENSE ACTIVATION (1 DEVICE PER LICENSE)
CREATE OR REPLACE FUNCTION public.activate_device_license(p_license_key TEXT, p_hwid TEXT)
RETURNS JSON AS $$
DECLARE
    v_lic RECORD;
BEGIN
    SELECT * INTO v_lic FROM public.licenses
    WHERE license_key = UPPER(TRIM(p_license_key));

    IF NOT FOUND THEN
        RETURN json_build_object('success', false, 'error_code', 'NOT_FOUND', 'message', 'Invalid license key. Please check your purchase receipt.');
    END IF;

    IF NOT v_lic.is_active THEN
        RETURN json_build_object('success', false, 'error_code', 'REVOKED', 'message', 'This license has been revoked or expired.');
    END IF;

    -- If already bound to another hardware device
    IF v_lic.bound_hwid IS NOT NULL AND v_lic.bound_hwid <> '' AND v_lic.bound_hwid <> TRIM(p_hwid) THEN
        RETURN json_build_object('success', false, 'error_code', 'DEVICE_LIMIT', 'message', 'DEVICE LIMIT EXCEEDED: This license is already bound to another PC. 1 license is valid for 1 device only.');
    END IF;

    -- Bind to this device if not yet bound
    UPDATE public.licenses
    SET bound_hwid = TRIM(p_hwid),
        last_used_at = TIMEZONE('utc'::text, NOW())
    WHERE id = v_lic.id;

    RETURN json_build_object('success', true, 'message', 'License verified and locked to this device successfully!');
END;
$$ LANGUAGE plpgsql SECURITY DEFINER;

